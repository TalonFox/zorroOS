const HAL = @import("hal");
const Thread = @import("root").Executive.Thread.Thread;
const Memory = @import("root").Memory;

pub const HCB = struct {
    activeUstack: u64 = 0,
    activeKstack: u64 = 0,
    activeIstack: u64 = 0,
    activeThread: ?*Thread = null,
    quantumsLeft: u32 = 0,
    hartID: i32 = 0,
    archData: HAL.Arch.ArchHCBData = HAL.Arch.ArchHCBData{},

    comptime {
        if (@offsetOf(@This(), "activeUstack") != 0) {
            @compileError("\"activeUstack\"'s offset is not: 0");
        }
        if (@offsetOf(@This(), "activeKstack") != 8) {
            @compileError("\"activeKstack\"'s offset is not: 8");
        }
        if (@offsetOf(@This(), "activeIstack") != 16) {
            @compileError("\"activeIstack\"'s offset is not: 16");
        }
    }
};
