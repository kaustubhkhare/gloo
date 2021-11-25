#mpirun -report-bindings -hostfile hosts -np 6 treebroadcast
mpirun -report-bindings -rankfile rankfile -np 4 treebroadcast --by-node