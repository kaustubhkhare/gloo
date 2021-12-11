#hosts=("pc281.emulab.net" "pc283.emulab.net" "pc306.emulab.net" "pc290.emulab.net" "pc288.emulab.net" "pc309.emulab.net" "pc295.emulab.net" "pc312.emulab.net" "pc305.emulab.net" "pc320.emulab.net" "pc308.emulab.net" "pc300.emulab.net")
hosts=("pc203.emulab.net" "pc201.emulab.net" "pc202.emulab.net" "pc204.emulab.net")

for i in "${!hosts[@]}"
do
  echo "Running $i -> ${hosts[$i]}"
  ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${hosts[$i]} "cd /proj/UWMadison744-F21/groups/akc/gloo && bash run.sh 4 ${i} enp9s4f0 treebroadcast" -t bash
done
