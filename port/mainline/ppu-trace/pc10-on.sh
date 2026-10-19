D=/sys/kernel/debug
for i in $(seq 0 15); do echo "r $((21346+i))" > $D/zx279133-smmu0; set -- $(cat $D/zx279133-smmu0); echo "w $((22258+i)) $2 $3 $4 $5" > $D/zx279133-smmu0; done
echo "0 0 7f e000f400 86000003 e041680d a0000000" > $D/zx279133-zcam
echo "5 0 8015" > $D/zx279133-spa
echo pc10-on
