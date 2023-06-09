const std = @import("std");
const HAL = @import("root").HAL;
const Memory = @import("root").Memory;
const Team = @import("root").Executive.Team;
const Thread = @import("root").Executive.Thread;
pub const PS2Keymap = @import("PS2Keymap.zig");

pub fn EnterDebugger() noreturn {
    // Prompt
    HAL.Console.Put("Welcome to the Ryu Kernel Debugger!\n\n", .{});
    while (true) {
        HAL.Console.Put("kdbg> ", .{});
        var buf: [256]u8 = [_]u8{0} ** 256;
        var i: usize = 0;
        while (true) {
            const key = HAL.Arch.DebugGet();
            if ((key == '\n') or (key != 8 and i < 256) or (key == 8 and i > 0)) {
                HAL.Console.Put("{c}", .{key});
            }
            if (key == '\n') {
                break;
            }
            if (key == 8) {
                if (i > 0) {
                    i -= 1;
                }
            } else {
                if (i < 256) {
                    buf[i] = key;
                    i += 1;
                }
            }
        }
        const txt = buf[0..i];
        if (std.mem.eql(u8, txt, "poolInfo")) {
            HAL.Console.Put("StaticPool | Buckets: {} UsedBlocks: {} FreeBlocks: {} TotalBlocks: {}\n", .{ Memory.Pool.StaticPool.buckets, Memory.Pool.StaticPool.usedBlocks, Memory.Pool.StaticPool.totalBlocks - Memory.Pool.StaticPool.usedBlocks, Memory.Pool.StaticPool.totalBlocks });
            HAL.Console.Put("           | Anonymous: {} KiB Committed: {} KiB Active: {} bytes\n", .{ Memory.Pool.StaticPool.anonymousPages * 4, (Memory.Pool.StaticPool.buckets * 512) + (Memory.Pool.StaticPool.anonymousPages * 4), (Memory.Pool.StaticPool.usedBlocks * 16) + (Memory.Pool.StaticPool.anonymousPages * 4096) });
            HAL.Console.Put("PagedPool  | Buckets: {} UsedBlocks: {} FreeBlocks: {} TotalBlocks: {}\n", .{ Memory.Pool.PagedPool.buckets, Memory.Pool.PagedPool.usedBlocks, Memory.Pool.PagedPool.totalBlocks - Memory.Pool.PagedPool.usedBlocks, Memory.Pool.PagedPool.totalBlocks });
            HAL.Console.Put("           | Anonymous: {} KiB Committed: {} KiB Active: {} bytes\n", .{ Memory.Pool.PagedPool.anonymousPages * 4, (Memory.Pool.PagedPool.buckets * 512) + (Memory.Pool.PagedPool.anonymousPages * 4), (Memory.Pool.PagedPool.usedBlocks * 16) + (Memory.Pool.PagedPool.anonymousPages * 4096) });
        } else if (std.mem.eql(u8, txt, "teams")) {
            var ind: i64 = 1;
            while (ind < Team.nextTeamID) : (ind += 1) {
                if (Team.teams.search(ind)) |team| {
                    HAL.Console.Put("{}: {x:0>16} {s}\n", .{ ind, @ptrToInt(&(team.value)), team.value.name });
                }
            }
        } else if (std.mem.eql(u8, txt, "threads")) {
            var ind: i64 = 1;
            while (ind < Thread.nextThreadID) : (ind += 1) {
                if (Thread.threads.search(ind)) |thread| {
                    HAL.Console.Put("{}: {x:0>16} {s} {s} IP: {x:0>16} SP: {x:0>16} Team #{}\n", .{
                        ind,
                        @ptrToInt(&(thread.value)),
                        thread.value.name,
                        @tagName(thread.value.state),
                        thread.value.context.GetReg(128),
                        thread.value.context.GetReg(129),
                        thread.value.team.teamID,
                    });
                }
            }
        } else {
            HAL.Console.Put("{s}?\n", .{txt});
        }
    }
}