cd ..
rm gloo.tar.gz
tar -czf gloo.tar.gz gloo
hosts=("c220g2-010628.wisc.cloudlab.us" "c220g2-010626.wisc.cloudlab.us" "c220g2-010631.wisc.cloudlab.us" "c220g2-010625.wisc.cloudlab.us")
for host in "${hosts[@]}"
do
  ssh -p 22 -l kkhare ${host} "sudo apt update; sudo apt install cmake -y;"
  ssh -p 22 -l kkhare ${host} "wget https://download.open-mpi.org/release/open-mpi/v4.1/openmpi-4.1.1.tar.gz; gunzip -c openmpi-4.1.1.tar.gz | tar xf -; cd openmpi-4.1.1; ./configure --prefix=/usr/local; sudo make all install; sudo ldconfig;"
  ssh -p 22 -l kkhare ${host} "cd openmpi-4.1.1; ./configure --prefix=/usr/local; sudo make all install; sudo ldconfig;"
done

