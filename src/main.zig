const std = @import("std");

const reminiscence = @import("reminiscence");

const Config = struct {
    framerate: u16,
    output_path: []const u8,

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
        try writer.print("process: {s}; pid: {}; wid: {}; geometry: {}", .{self.name, self.pid, self.wid, self.geometry});
    }
};

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    const allocator = gpa.allocator();

    const datestr = try get_datestr(allocator);
    defer allocator.free(datestr);

    const config = Config{
        .framerate = 60, 
        .output_path = try std.fmt.allocPrint(allocator, "Recordings/{s}.mp4", .{datestr})};

    defer allocator.free(config.output_path);

    var processes = try get_processes(allocator);
    defer processes.deinit(allocator);
    defer {
        for (processes.items) |p| {
            p.deinit(allocator);
        }
    }

    std.debug.print("Found {} process/es\n", .{processes.items.len});
    for (processes.items) |p| {
        std.debug.print("{f}\n", .{p});
    }

    // var args = .{
    //     "wf-recorder", 
    //     "-f", config.output_path, 
    //     "--framerate", try config.framerate_as_str(allocator), 
    //     "--overwrite"
    // };
    // var child = std.process.Child.init(&args, allocator);
    //
    // try child.spawn();
    // _ = try child.wait();
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
        // _ = line_iter.next().?;
        // std.debug.print("{s}\n", .{line_iter.next().?});
        // we have to make copies of the strings or not free them above
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
