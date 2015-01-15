struct _coord {
    uint32_t x, y;
    uint8_t finger;
};

struct _ts_event {
    uint8_t  n_fingers;
    struct _coord coords[5];
};
