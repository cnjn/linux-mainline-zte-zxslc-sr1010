#!/bin/sh

set -u

sample=/tmp/sfc-read-smoke.bin
failed=0

read_counter()
{
	if [ -r "$1" ]; then
		cat "$1"
	else
		echo 0
	fi
}

for index in 0 1 2 3 4 5 6; do
	sys=/sys/class/mtd/mtd$index
	dev=/dev/mtd$index
	name=$(cat "$sys/name")
	size=$(cat "$sys/size")
	flags=$(cat "$sys/flags")
	blocks=$((size / 4096))
	middle=$((blocks / 2))
	last=$((blocks - 16))
	bad_before=$(read_counter "$sys/bad_blocks")
	bbt_before=$(read_counter "$sys/bbt_blocks")
	fail_before=$(read_counter "$sys/ecc_failures")
	corr_before=$(read_counter "$sys/corrected_bits")

	case "$flags" in
	0|0x0|0x00000000) ;;
	*)
		echo "MTD index=$index name=$name flags=$flags writable FAIL"
		failed=1
		;;
	esac

	for entry in begin:0 middle:$middle end:$last; do
		label=${entry%%:*}
		offset=${entry#*:}
		first=
		second=
		for round in 1 2; do
			if ! dd if="$dev" of="$sample" bs=4096 skip="$offset" \
				count=16 2>/dev/null; then
				echo "MTD index=$index name=$name sample=$label round=$round read FAIL"
				failed=1
				continue
			fi
			bytes=$(wc -c < "$sample")
			hash=$(sha256sum "$sample" | awk '{print $1}')
			if [ "$bytes" -ne 65536 ]; then
				echo "MTD index=$index name=$name sample=$label round=$round bytes=$bytes FAIL"
				failed=1
			fi
			if [ "$round" -eq 1 ]; then
				first=$hash
			else
				second=$hash
			fi
		done
		if [ -n "$first" ] && [ "$first" = "$second" ]; then
			echo "MTD index=$index name=$name sample=$label bytes=65536 hash=$first PASS"
		else
			echo "MTD index=$index name=$name sample=$label repeat-mismatch FAIL"
			failed=1
		fi
	done

	bad_after=$(read_counter "$sys/bad_blocks")
	bbt_after=$(read_counter "$sys/bbt_blocks")
	fail_after=$(read_counter "$sys/ecc_failures")
	corr_after=$(read_counter "$sys/corrected_bits")
	if [ "$bad_before:$bbt_before:$fail_before" = \
	     "$bad_after:$bbt_after:$fail_after" ]; then
		echo "MTD index=$index bad=$bad_after bbt=$bbt_after ecc_fail=$fail_after corrected=$corr_before->$corr_after PASS"
	else
		echo "MTD index=$index bad=$bad_before->$bad_after bbt=$bbt_before->$bbt_after ecc_fail=$fail_before->$fail_after FAIL"
		failed=1
	fi
done

rm -f "$sample"
if [ "$failed" -eq 0 ]; then
	echo "SFC_READ_SMOKE PASS"
else
	echo "SFC_READ_SMOKE FAIL"
fi
exit "$failed"
