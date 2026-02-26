if (variable_global_exists("qr_sprite")) {
    var spr = global.qr_sprite;
    if (is_real(spr) && spr != -1) {
        sprite_delete(spr);
        global.qr_sprite = -1;
    }
}
if (file_exists("qr_tmp.png")) file_delete("qr_tmp.png");
