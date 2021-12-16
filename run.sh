mkdir -p ~/logs
cd /proj/UWMadison744-F21/groups/akc/gloo/build
iters=(30000 20000 13333 8888 5925 3950 2633 1500 1000)
#iters=(1, 1)
#iters=(1 1 1 1 1 1 1 1 1)
cnt=(10 100 500 1000 5000 20000 80000 100000 140000)
#cnt=(2000000, 5000000)
rm ~/logs/${4}.log
#netw=$((ifconfig | grep -B1 $(ping -c1 $(hostname | cut -d "." -f1) | head -1 | cut -d " " -f3 | cut -c2- | rev | cut -c2- | rev)) | head -1 | cut -d " " -f1 | rev | cut -c2- | rev)
for i in `seq 0 8`; do
  echo "Running: PREFIX=${4}-${i}${i} SIZE=${1} RANK=${2} NETWORK=${3} INPUT_SIZE=${cnt[$i]} ITERS=${iters[$i]} $(find . -name $4) &> ~/logs/${4}.log &" >> ~/logs/${4}.log
  PREFIX="${4}-${i}${i}" SIZE=$1 RANK=$2 NETWORK=${3} INPUT_SIZE=${cnt[$i]} ITERS=${iters[$i]} $(find . -name $4) &>> ~/logs/${4}.log
  echo ""
  echo ""
done

#PREFIX=test2 SIZE=4 RANK=0 NETWORK=enp9s4f0 ./treebroadcast
