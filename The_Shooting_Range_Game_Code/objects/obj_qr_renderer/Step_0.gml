if(global.qrCodeUsed){
	alpha -= 0.05;
	if(alpha <= 0){
		show_debug_message("[QR] Credentials received, removing QR code.")
		instance_destroy();
	}
}