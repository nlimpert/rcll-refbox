
;---------------------------------------------------------------------------
;  game.clp - LLSF RefBox CLIPS game maintenance
;
;  Created: Tue Jun 11 15:19:25 2013
;  Copyright  2013  Tim Niemueller [www.niemueller.de]
;  Licensed under BSD license, cf. LICENSE file
;---------------------------------------------------------------------------

(deffunction game-reset ()

  ; Retract machine initialization state, if set
  (do-for-fact ((?mi machines-initialized)) TRUE
    (retract ?mi)
  )

  ; retract all delivery periods
  ;(printout t "Retracting delivery periods" crlf)
  (delayed-do-for-all-facts ((?dp delivery-period)) TRUE
    (retract ?dp)
  )

  ; reset all down periods
  ;(printout t "Resetting down periods" crlf) 
  (delayed-do-for-all-facts ((?m machine)) TRUE
    (modify ?m (down-period (deftemplate-slot-default-value machine down-period)))
  )
)

(deffunction game-calc-phase-points (?team-color ?phase)
  (bind ?phase-points 0)
  (do-for-all-facts ((?p points)) (and (eq ?p:phase ?phase) (eq ?p:team ?team-color))
    (bind ?phase-points (+ ?phase-points ?p:points))
  )
  (return ?phase-points)
)

(deffunction game-calc-points (?team-color)
  (bind ?points 0)
  (foreach ?phase (deftemplate-slot-allowed-values points phase)
    (bind ?phase-points (game-calc-phase-points ?team-color ?phase))
    (bind ?points (+ ?points (max ?phase-points 0)))
  )
  (return ?points)
)

(defrule game-reset
  (game-reset) ; this is a fact
  =>
  (game-reset) ; this is a function
)

(defrule game-parameterize
  (declare (salience ?*PRIORITY_HIGHER*))
  (gamestate (phase SETUP|EXPLORATION|PRODUCTION) (prev-phase PRE_GAME))
  (not (game-parameterized))
  (not (sync)) ; will be initialized in rule sync-slave-receive
  =>
  (assert (game-parameterized))

  ; machine assignment if not already done
  (if (not (any-factp ((?mi machines-initialized)) TRUE))
   then (machine-init-randomize))

  ; reset late orders, assign random times
  (delayed-do-for-all-facts ((?order order)) (eq ?order:late-order TRUE)
    (bind ?deliver-start
      (random (nth$ 1 ?order:late-order-start-period)
	      (nth$ 2 ?order:late-order-start-period)))
    (bind ?deliver-end (+ ?deliver-start ?*LATE-ORDER-TIME*))
    (bind ?activate-at (max (- ?deliver-start ?*LATE-ORDER-ACTIVATION-PRE-TIME*) 0))
    (modify ?order (active FALSE) (activate-at ?activate-at)
	    (delivery-period ?deliver-start ?deliver-end))
  )

  ; make sure T5 machines are not down during late orders
  (do-for-all-facts ((?machine machine) (?spec machine-spec))
    (and (eq ?machine:mtype T5) (eq ?spec:mtype T5) (>= (nth$ 1 ?machine:down-period) 0.0))

    (bind ?down-start (nth$ 1 ?machine:down-period))
    (bind ?down-end   (nth$ 2 ?machine:down-period))

    ;(printout warn "Checking T5 " ?machine:name "(" ?down-start " to " ?down-end ")" crlf)

    (do-for-all-facts ((?order order)) ?order:late-order
      (bind ?order-start (nth$ 1 ?order:delivery-period))
      (bind ?order-end   (nth$ 2 ?order:delivery-period))
      (if (and (> ?order-end ?down-start) (<= ?order-end ?down-end))
       then
        ; the end of the order time is within the down time
        ; push down-time back, and shrink if necessary to not exceed game time.
        ; this might even eliminate the down time, lucky team I guess...
        (bind ?new-down-start ?order-end)
	(bind ?new-down-end
	      (min (+ ?new-down-start (- ?down-end ?down-start)) ?*PRODUCTION-TIME*))
	(printout t "Late order down-time conflict (1) for " ?machine:name "|T5" crlf)
	(printout t "New downtime for " ?machine:name ": "
		  (time-sec-format ?new-down-start) " to " (time-sec-format ?new-down-end)
		  " (was " (time-sec-format ?down-start) " to "
		  (time-sec-format ?down-end) ")" crlf)
	(modify ?machine (down-period ?new-down-start ?new-down-end))
       else
        (if (and (>= ?order-start ?down-start) (< ?order-start ?down-end))
          then
          ; the start of the order time is within the down time
          ; pull down-time forward, and shrink if necessary to not exceed game time.
          ; this might even eliminate the down time, lucky team I guess...
          (bind ?new-down-end ?order-start)
	  (bind ?new-down-start
		(max (- ?new-down-end (- ?down-end ?down-start)) 0))
	  (printout t "Late order down-time conflict (2) for " ?machine:name "|T5" crlf)
	  (printout t "New downtime for " ?machine:name ": "
		    (time-sec-format ?new-down-start) " to " (time-sec-format ?new-down-end)
		    " (was " (time-sec-format ?down-start) " to "
		    (time-sec-format ?down-end) ")" crlf)
	  (modify ?machine (down-period ?new-down-start ?new-down-end))
        )
      )
    )

    ; assign random quantities to non-late orders
    (delayed-do-for-all-facts ((?order order)) (neq ?order:late-order TRUE)
      (modify ?order (quantity-requested (random ?*ORDER-QUANTITY-MIN* ?*ORDER-QUANTITY-MAX*)))
    )
  )
)

(defrule game-reset-print
  (game-reset)
  ?gf <- (game-printed)
  =>
  (retract ?gf)
)
  

(defrule game-print
  (game-parameterized)
  (gamestate (teams $?teams))
  (not (game-printed))
  =>
  (assert (game-printed))

  (bind ?t t)
  (if (neq ?teams (create$ "" ""))
   then
    (bind ?t debug)
    (printout warn "Printing game config to debug log only" crlf)
  )

  ; Print late orders
  (do-for-all-facts ((?order order)) ?order:late-order
    (printout ?t (if ?order:late-order then "Late " else "") "Order " ?order:id
	      ": from " (time-sec-format (nth$ 1 ?order:delivery-period))
	      " to " (time-sec-format (nth$ 2 ?order:delivery-period)) crlf)
  )
)

(defrule game-update-gametime-points
  (declare (salience ?*PRIORITY_FIRST*))
  (time $?now)
  ?gf <- (gamestate (phase SETUP|EXPLORATION|PRODUCTION|WHACK_A_MOLE_CHALLENGE|NAVIGATION_CHALLENGE)
		    (state RUNNING)
		    (game-time ?game-time) (cont-time ?cont-time)
		    (last-time $?last-time&:(neq ?last-time ?now)))
  =>
  (bind ?points-cyan (game-calc-points CYAN))
  (bind ?points-magenta (game-calc-points MAGENTA))
  (bind ?timediff (time-diff-sec ?now ?last-time))
  (modify ?gf (game-time (+ ?game-time ?timediff)) (cont-time (+ ?cont-time ?timediff))
	  (last-time ?now) (points ?points-cyan ?points-magenta))
)

(defrule game-update-last-time
  (declare (salience ?*PRIORITY_FIRST*))
  (time $?now)
  (or (gamestate (phase ~PRODUCTION&~EXPLORATION&~SETUP&~WHACK_A_MOLE_CHALLENGE&~NAVIGATION_CHALLENGE))
      (gamestate (state ~RUNNING)))
  ?gf <- (gamestate (last-time $?last-time&:(neq ?last-time ?now)))
  =>
  (modify ?gf (last-time ?now))
)


(defrule game-switch-to-pre-game
  ?gs <- (gamestate (phase PRE_GAME) (prev-phase ~PRE_GAME))
  =>
  (modify ?gs (prev-phase PRE_GAME) (game-time 0.0) (state WAIT_START))
  (delayed-do-for-all-facts ((?machine machine)) TRUE
    (modify ?machine (desired-lights GREEN-ON YELLOW-ON RED-ON))
  )
  ;(assert (reset-game))
)

(defrule game-start-training
  ?gs <- (gamestate (teams "" "") (phase PRE_GAME) (state RUNNING))
  =>
  (modify ?gs (phase EXPLORATION) (prev-phase PRE_GAME) (game-time 0.0) (start-time (now)))
  (assert (attention-message (text "Starting  *** TRAINING ***  game")))
)

(defrule game-start
  ?gs <- (gamestate (teams ?team_cyan ?team_magenta) (phase PRE_GAME) (state RUNNING))
  =>
  (modify ?gs (phase SETUP) (prev-phase PRE_GAME) (start-time (now)))
  (assert (attention-message (text "Starting setup phase") (time 15)))
)

(defrule game-setup-warn-end-near
  (gamestate (phase SETUP) (state RUNNING)
	     (game-time ?game-time&:(>= ?game-time (* ?*SETUP-TIME* .9))))
  (not (setup-warned))
  =>
  (assert (setup-warned))
  (assert (attention-message (text "Setup phase is about to end")))
)

(defrule game-switch-to-exploration
  ?gs <- (gamestate (phase SETUP) (state RUNNING)
		    (game-time ?game-time&:(>= ?game-time ?*SETUP-TIME*)))
  =>
  (modify ?gs (phase EXPLORATION) (prev-phase SETUP) (game-time 0.0))
  (assert (attention-message (text "Switching to exploration phase")))
)

(defrule game-switch-to-production
  ?gs <- (gamestate (phase EXPLORATION) (state RUNNING)
		    (game-time ?game-time&:(>= ?game-time ?*EXPLORATION-TIME*)))
  =>
  (modify ?gs (phase PRODUCTION) (prev-phase EXPLORATION) (game-time 0.0))
  (assert (attention-message (text "Switching to production phase")))
)

(deffunction game-print-points-team (?team)
  (bind ?points 0)
  (printout t "-- Awarded Points -- " ?team " --" crlf)
  (foreach ?phase (deftemplate-slot-allowed-values points phase)
    (if (any-factp ((?p points)) (and (eq ?phase ?p:phase) (eq ?team ?p:team) (> ?p:points 0)))
    then
      (printout t ?phase crlf)
      (bind ?phase-points 0)
      (do-for-all-facts ((?p points)) (and (eq ?p:team ?team) (eq ?p:phase ?phase))
        (printout t
          (format nil "  %s  %2d  %s" (time-sec-format ?p:game-time) ?p:points ?p:reason) crlf)
        (bind ?phase-points (+ ?phase-points ?p:points))
      )
      (printout t ?phase " TOTAL: " ?phase-points crlf)
      (bind ?points (+ ?points ?phase-points))
    )
  )
  (printout t "OVERALL TOTAL POINTS: " ?points crlf)
)

(deffunction game-print-points ()
  (game-print-points-team CYAN)
  (game-print-points-team MAGENTA)
)

(defrule game-over
  ?gs <- (gamestate (refbox-mode STANDALONE) (phase PRODUCTION) (state RUNNING)
		    (over-time FALSE)
		    (game-time ?game-time&:(>= ?game-time ?*PRODUCTION-TIME*)))
  =>
  (modify ?gs (phase POST_GAME) (prev-phase PRODUCTION) (state PAUSED) (end-time (now)))
)

(defrule game-over-after-overtime
  ?gs <- (gamestate (refbox-mode ~STANDALONE) (phase PRODUCTION) (state RUNNING)
		    (over-time TRUE)
		    (game-time ?gt&:(>= ?gt (+ ?*PRODUCTION-TIME* ?*PRODUCTION-OVERTIME*))))
  =>
  (modify ?gs (phase POST_GAME) (prev-phase PRODUCTION) (state PAUSED) (end-time (now)))
)

(defrule game-over-waitsync
  ?gs <- (gamestate (refbox-mode ~STANDALONE) (phase PRODUCTION) (state RUNNING)
		    (over-time FALSE) (game-time ?gt&:(>= ?gt ?*PRODUCTION-TIME*)))
  =>
  (modify ?gs (state PAUSED) (end-time (now)))
  (assert (attention-message (text "Waiting for synchronized game to end")
			     (time ?*PRODUCTION-TIME*)))
)

(defrule game-goto-post-game
  ?gs <- (gamestate (phase POST_GAME) (prev-phase ~POST_GAME))
  =>
  (modify ?gs (prev-phase POST_GAME))
  (delayed-do-for-all-facts ((?machine machine)) TRUE
    (modify ?machine (desired-lights RED-BLINK))
  )
  (game-print-points)
  (assert (attention-message (text "Game Over") (time 60)))
  (printout t "===  Game Over  ===" crlf)
)
