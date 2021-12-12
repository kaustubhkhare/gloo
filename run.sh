mkdir -p ~/logs
cd /proj/UWMadison744-F21/groups/akc/gloo/build
echo "Running: PREFIX=${4} SIZE=${1} RANK=${2} NETWORK=${3} VSIZE=${5} $(find . -name $4) &> ~/logs/${4}.log &" > ~/logs/${4}.log
PREFIX=$4 SIZE=$1 RANK=$2 NETWORK=$3 VSIZE=$5 $(find . -name $4) &>> ~/logs/${4}.log &

#PREFIX=test2 SIZE=4 RANK=0 NETWORK=enp9s4f0 ./treebroadcast
