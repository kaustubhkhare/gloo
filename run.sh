mkdir -p ~/logs
cd /proj/UWMadison744-F21/groups/akc/gloo/build
#netw=$((ifconfig | grep -B1 $(ping -c1 $(hostname | cut -d "." -f1) | head -1 | cut -d " " -f3 | cut -c2- | rev | cut -c2- | rev)) | head -1 | cut -d " " -f1 | rev | cut -c2- | rev)
echo "Running: PREFIX=${4} SIZE=${1} RANK=${2} NETWORK=${3} INPUT_SIZE=${5} ITERS=${6} $(find . -name $4) &> ~/logs/${4}.log &" > ~/logs/${4}.log
PREFIX=$4 SIZE=$1 RANK=$2 NETWORK=${3} INPUT_SIZE=$5 ITERS=${6} $(find . -name $4) &>> ~/logs/${4}.log &

#PREFIX=test2 SIZE=4 RANK=0 NETWORK=enp9s4f0 ./treebroadcast
