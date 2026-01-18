const std = @import("std");

const reminiscence = @import("reminiscence");

const Config = struct {
    framerate: u16,
    output_dir: []const u8,

    fn framerate_as_str(self: Config, allocator: std.mem.Allocator) ![]const u8 {
        return try std.fmt.allocPrint(allocator, "{}", .{self.framerate});
    }
};

const Process = struct {
    name: []const u8,
    machine_name: []const u8,
    pid: u16,
    wid: u32,
    desktop_id: u16,
    geometry: struct {
        x_offset: i32,  
        y_offset: i32,  
        width: u32,
        height: u32,
    },

    pub fn deinit(self: Process, allocator: std.mem.Allocator) void {
        allocator.free(self.name);
        allocator.free(self.machine_name);
    }

    pub fn format(self: Process, writer: *std.io.Writer) !void {
        try writer.print("{s}; pid: {}; wid: {}; geometry: {}", .{self.name, self.pid, self.wid, self.geometry});
    }

    pub fn eql(item1: Process, item2: Process) bool {
        if (!std.mem.eql(u8, item1.name, item2.name)) return false;
        if (item1.pid != item2.pid) return false;

        return true;
    }
};

const NS_PER_MS: u64 = 1_000_000;

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const config = Config{
        .framerate = 60, 
        .output_dir = "./Recordings/",
    };

    while (true) {
        var processes = try get_processes(allocator);
        defer processes.deinit(allocator);
        defer {
            for (processes.items) |p| {
                p.deinit(allocator);
            }
        }

        if (processes.items.len > 0) {
            std.debug.print("Found {} process/es\n", .{processes.items.len});
            for (processes.items) |p| {
                std.debug.print("{f}\n", .{p});
            }

            const process = &processes.items[0];
            var rec: Recorder = try Recorder.init(allocator, process, &config);
            try rec.start_recording();
            std.debug.print("Starting recording with process: {f}\n", .{process});

            // Break this into another fn?
            // Keep recording until our original process is no longer running
            while (true) {
                var processes2 = try get_processes(allocator);
                defer processes2.deinit(allocator);

                var still_running: bool = false;
                for (processes2.items) |p2| {
                    if (Process.eql(process.*, p2)) {
                        still_running = true;
                        break;
                    }
                }
                if (!still_running) break;

                std.Thread.sleep(1000 * NS_PER_MS);
            }

            try rec.stop_recording();
            std.debug.print("Stopped recording for process {f}\n", .{process});
        }
        else {
            std.debug.print("No processes found\n", .{});
        }

        std.Thread.sleep(1000 * NS_PER_MS);
    }
}

fn get_datestr(allocator: std.mem.Allocator) ![]u8 {
    const epoch_seconds = std.time.epoch.EpochSeconds{ .secs = @intCast(std.time.timestamp()) };
    const epoch_day = epoch_seconds.getEpochDay();
    const day_seconds = epoch_seconds.getDaySeconds();
    const year_day = epoch_day.calculateYearDay();
    const month_day = year_day.calculateMonthDay();

    return try std.fmt.allocPrint(allocator, "{}-{}-{}-{}-{}", 
        .{year_day.year, month_day.month.numeric(), month_day.day_index+1, day_seconds.getHoursIntoDay(), day_seconds.getMinutesIntoHour()});
}

fn get_processes(allocator: std.mem.Allocator) !std.ArrayList(Process) {
    var processes: std.ArrayList(Process) = .empty;

    const args = .{
        "wmctrl",
        "-l",
        "-p",
        "-G"
    };

    const result = try std.process.Child.run(.{
        .allocator = allocator,
        .argv = &args,
    });
    defer allocator.free(result.stdout);
    defer allocator.free(result.stderr);

    // This is the output format
    // 0x0240004a  0 13832  1460 740  1100 700  omarchy Steam
    var iter = std.mem.splitScalar(u8, result.stdout, '\n');
    while (iter.next()) |line| {
        if (line.len == 0) continue;
        var line_iter = std.mem.tokenizeScalar(u8, line, ' ');
        const process = Process{
            .wid = try std.fmt.parseInt(u32, line_iter.next().?, 0),
            .desktop_id = try std.fmt.parseInt(u16, line_iter.next().?, 10),
            .pid = try std.fmt.parseInt(u16, line_iter.next().?, 10),
            .geometry = .{
                .x_offset = try std.fmt.parseInt(i32, line_iter.next().?, 10),
                .y_offset = try std.fmt.parseInt(i32, line_iter.next().?, 10),
                .width = try std.fmt.parseInt(u32, line_iter.next().?, 10),
                .height = try std.fmt.parseInt(u32, line_iter.next().?, 10),
            },
            .machine_name = try allocator.dupe(u8, line_iter.next().?),
            .name = try allocator.dupe(u8, line_iter.rest()),
        };
        
        try processes.append(allocator, process);
    }

    return processes;
}

const Recorder = struct {
    child: ?std.process.Child = null,
    child_args: [][]const u8,

    pub fn init(allocator: std.mem.Allocator, process: *const Process, config: *const Config) !Recorder {
        const datestr = try get_datestr(allocator);
        defer allocator.free(datestr);

        const full_path = try std.fmt.allocPrint(allocator, "{s}/{s}-{s}.mp4", .{config.output_dir, process.name, datestr});
        defer allocator.free(full_path);

        // TODO: We have to make this args same lifetime as our struct then free it in the deinit
        const args_tmp = [_][]const u8 {
            "wf-recorder", 
            "-f", full_path, 
            "--framerate", try config.framerate_as_str(allocator), 
            "--overwrite"
        };

        var args = try allocator.alloc([]const u8, args_tmp.len);
        for (args_tmp, 0..) |arg, i| {
            args[i] = try allocator.dupe(u8, arg);
        }

        const child = std.process.Child.init(args, allocator);

        return .{
            .child = child,
            .child_args = args,
        };
    }

    // TODO: Create deinit fn

    pub fn start_recording(self: *Recorder) !void {
        _ = try self.child.?.spawn();
    }

    pub fn stop_recording(self: *Recorder) !void {
        _ = try self.child.?.kill();
    }
};
