#!/usr/bin/env bash
# Kill autorun_app on all autorun_nodes

# Request autorun to generate the list of nodes so that we can kill.
# The list can be in any order.
autorun_gen_nodes=1

# Do not overwrite the nodemap previously generated by run-all.sh
overwrite_nodemap=0
source $(dirname $0)/autorun.sh

for node in $autorun_nodes; do
	blue "kill-all: Killing $autorun_app on $node"
	ssh -oStrictHostKeyChecking=no $node "sudo killall $autorun_app" &
done

wait
