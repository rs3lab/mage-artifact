namespace cpp mind_ctrl

struct Command {
    1: i32 id,
    2: string data,
}

service MindCtrl {
    Command exchange(1: Command cmd);
}
