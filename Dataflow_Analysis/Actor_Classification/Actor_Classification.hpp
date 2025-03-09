#pragma once


enum class Actor_Classification {
	static_rate,
	/* Actor has a FSM that has cycles (all of the same length) and in parallel states
	 * the token rates per channel are the same. 
	 */
	cyclostatic_rate,
	/* It has only a single cycle, this is required for parallelization as in this case
	   no decision from the previous firing is needed.
	   Within one state there can still be many actions, but the scheduling is limited to
	   local scheduling without dependencies to the state, as the state is known in
	   single cycle cyclostatic dataflow.
	*/
	singlecycle_cyclostatic_rate,
	/* In each state all actions consume/produce a static amount of tokens. */
	state_static,
	/* None of the others, tokens are consumed/produce with different rates per channel. */
	dynamic_rate,
};
