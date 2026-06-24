PART_NAME=firmware

platform_check_image() {
	[ "$#" -gt 1 ] && {
		echo "Only one image file can be specified"
		return 1
	}

	case "$(get_magic_long "$1")" in
		27051956)
			return 0
			;;
		*)
			echo "Invalid image type. Please use only sysupgrade.bin files"
			return 1
			;;
	esac
}

platform_do_upgrade() {
	default_do_upgrade "$1"
}
