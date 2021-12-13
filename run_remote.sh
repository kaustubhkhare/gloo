#hosts=("pc791.emulab.net" "pc793.emulab.net" "pc781.emulab.net"  "pc802.emulab.net"  "pc809.emulab.net"  "pc795.emulab.net"  "pc786.emulab.net"  "pc785.emulab.net"  "pc782.emulab.net"  "pc788.emulab.net"  "pc811.emulab.net"  "pc796.emulab.net")
#ifaces=("enp6s0f1" "enp6s0f1" "enp6s0f0" "enp6s0f0" "enp6s0f0" "enp4s0f0" "enp4s0f1" "enp4s0f1" "enp6s0f1" "enp4s0f1" "enp6s0f0" "enp6s0f0")

hosts=("pc791.emulab.net" "pc793.emulab.net" "pc781.emulab.net"  "pc802.emulab.net"  "pc809.emulab.net"  "pc795.emulab.net"  "pc786.emulab.net"  "pc785.emulab.net"  "pc782.emulab.net"  "pc788.emulab.net"  "pc811.emulab.net"  "pc796.emulab.net")
ifaces=("enp6s0f1" "enp6s0f1" "enp6s0f0" "enp6s0f0" "enp6s0f0" "enp4s0f0" "enp4s0f1" "enp4s0f1" "enp6s0f1" "enp4s0f1" "enp6s0f0" "enp6s0f0")

#hosts=("pc791.emulab.net" "pc793.emulab.net" "pc781.emulab.net"  "pc802.emulab.net"  "pc809.emulab.net"  "pc795.emulab.net"  "pc786.emulab.net"  "pc785.emulab.net"  "pc782.emulab.net" "pc811.emulab.net" "pc788.emulab.net"  "pc796.emulab.net")
#ifaces=("enp6s0f1" "enp6s0f1" "enp6s0f0" "enp6s0f0" "enp6s0f0" "enp4s0f0" "enp4s0f1" "enp4s0f1" "enp6s0f1" "enp6s0f0" "enp4s0f1" "enp6s0f0")

size=8
lessthansize=7
#binary=sagbroadcast
binary=treereduce
#checkpointdir=rendezvous_checkpoint
checkpointdir=rendezvous_checkpoint-CT
ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${hosts[0]} "rm /proj/UWMadison744-F21/groups/akc/${checkpointdir}/*"
ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${hosts[0]} "cd /proj/UWMadison744-F21/groups/akc/gloo/ && git pull && cd /proj/UWMadison744-F21/groups/akc/gloo/build/ && make -j4"
for i in "${!hosts[@]}"
do
  if [[ "$i" -le $lessthansize ]]
  then
    echo "Running $i -> ${hosts[$i]}"
    ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${hosts[$i]} "cd /proj/UWMadison744-F21/groups/akc/gloo && bash run.sh ${size} ${i} ${ifaces[$i]} ${binary} 16 20" &> log.log &
  fi
done
wait