cd ..
rm gloo.tar.gz
tar -czf gloo.tar.gz gloo
hosts=("c220g2-010628.wisc.cloudlab.us" "c220g2-010626.wisc.cloudlab.us" "c220g2-010631.wisc.cloudlab.us" "c220g2-010625.wisc.cloudlab.us")
for host in "${hosts[@]}"
do
    ssh -p 22 -l kkhare ${host} "rm -rf gloo.tar.gz gloo"
    scp -P 22 gloo.tar.gz kkhare@${host}:/users/kkhare/.
    ssh -p 22 -l kkhare ${host} "tar -xzf gloo.tar.gz"
    ssh -p 22 -l kkhare ${host} "cd gloo; mkdir build; cd build; cmake -DBUILD_EXAMPLES=1 ../; make -j4"
done

