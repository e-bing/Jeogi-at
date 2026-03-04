case CMD_SET_CONGESTION: {
    if (rxPkt.len == 2) {
        uint8_t p_id = rxPkt.data[0];
        uint8_t c_lv = rxPkt.data[1];
        // 이 로그가 30초마다 8줄씩 찍히면 성공입니다.
        printf("[UART6] Platform: %d, Congestion: %d\r\n", p_id, c_lv);
    }
    break;
}
