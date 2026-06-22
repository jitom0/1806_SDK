PART_NAME=firmware

platform_check_image() {
	return 0

	[ "$#" -gt 1 ] && {
		echo "Only one image file can be specified"
		return 1
	}

	local board="$(board_name)"

	if [ -z "$board" ] || [ "$board" = "generic" ]; then
		echo "Unable to determine board name for sysupgrade validation."
		return 1
	fi

	nand_do_platform_check "$board" "$1"
}

platform_do_upgrade() {
	nand_do_upgrade "$1"
}
