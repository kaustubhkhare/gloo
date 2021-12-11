#hosts=("pc203.emulab.net" "pc201.emulab.net" "pc202.emulab.net" "pc204.emulab.net")
hosts=("pc791.emulab.net"  "pc793.emulab.net"  "pc781.emulab.net"  "pc802.emulab.net"  "pc809.emulab.net"  "pc795.emulab.net"  "pc786.emulab.net"  "pc785.emulab.net"  "pc782.emulab.net"  "pc788.emulab.net"  "pc811.emulab.net"  "pc796.emulab.net")
rm all-keys.txt
for host in "${hosts[@]}"
do
  ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${host} 'cat /dev/zero | ssh-keygen -q -N ""'
  scp -p 22 -o StrictHostKeyChecking=no kkhare@${host}:~/.ssh/id_rsa.pub .
  cat id_rsa.pub >> all-keys.txt
done

for host in "${hosts[@]}"
do
  scp -p 22 -o StrictHostKeyChecking=no  all-keys.txt kkhare@${host}:/tmp/.
  ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${host} "cat /tmp/all-keys.txt >> ~/.ssh/authorized_keys"
done
