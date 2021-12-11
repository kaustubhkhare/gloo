#cd ..
#rm gloo.tar.gz
#tar -czf gloo.tar.gz gloo
#hosts=("pc281.emulab.net" "pc283.emulab.net" "pc306.emulab.net" "pc290.emulab.net" "pc288.emulab.net" "pc309.emulab.net" "pc295.emulab.net" "pc312.emulab.net" "pc305.emulab.net" "pc320.emulab.net" "pc308.emulab.net" "pc300.emulab.net")
#hosts=("pc203.emulab.net" "pc201.emulab.net" "pc202.emulab.net" "pc204.emulab.net")
hosts=("pc791.emulab.net")
for host in "${hosts[@]}"
do

  ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${host} "rm -rf openmpi-4.1.1"
  ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${host} "rm openmpi-4.1.1.tar.gz"
  ((ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${host} "sudo apt update; sudo apt install cmake -y;" &> install-logs/${host}.cmake.log) && (ssh -o StrictHostKeyChecking=no -p 22 -l kkhare ${host} "wget https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.1.tar.gz; gunzip -c openmpi-4.1.1.tar.gz | tar xf -; cd openmpi-4.1.1; ./configure --prefix=/usr/local ; sudo make all install -j10; sudo ldconfig;" &> install-logs/${host}.log))&
#  ssh -p 22 -l kkhare ${host} "cd openmpi-4.1.1; ./configure --prefix=/usr/local; sudo make all install; sudo ldconfig;"
done

