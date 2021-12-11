mkdir -p ~/logs
rm /proj/UWMadison744-F21/groups/akc/rendezvous_checkpoint/*
cd /proj/UWMadison744-F21/groups/akc/gloo/build
PREFIX=$4 SIZE=$1 RANK=$2 NETWORK=$3 $(find . -name $4) &> ~/logs/${4}.log &
