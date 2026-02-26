function register__version_ms() {
    if (!variable_global_exists("register_seq")) global.register_seq = 0;
    global.register_seq += 1;
    // current_time = ms since game start (always available)
    return current_time + global.register_seq;
}
