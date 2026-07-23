// const std = @import("std");
// const Atomic = std.atomic.Value;

// pub const Job = struct {
//     task_fn: *const fn (ctx: *anyopaque) void,
//     ctx: anyopaque,

//     // How many parent jobs must finish before this runs?
//     unmet_dependencies: Atomic(u32),

//     // Which jobs are waiting on this one?
//     dependents: [8]*Job,
//     dependents_count: usize,

//     pub fn execute(self: *Job) void {
//         self.task_fn(self.ctx);
//     }
// };

// pub const JobGraph = struct {
//     allocator: std.mem.Allocator,

//     // Queue of jobs that have 0 unmet dependencies and are ready to run
//     ready_queue: *LockFreeQueue(*Job),

//     pub fn addJob(self: *JobGraph, task_fn: *const fn (*anyopaque) void, ctx: *anyopaque) *Job {
//         const job = self.allocator.create(Job) catch unreachable;
//         job.* = .{
//             .task_fn = task_fn,
//             .ctx = ctx,
//             .unmet_dependencies = Atomic(u32).init(0),
//             .dependent_count = 0,
//         };
//         return job;
//     }

//     pub fn addDependency(parent: *Job, child: *Job) void {
//         // The child needs to wait for one more thing
//         _ = child.unmet_dependencies.fetchAdd(1, .monotonic);

//         // The parent knows it must notify the child
//         parent.dependents[parent.dependent_count] = child;
//         parent.dependent_count += 1;
//     }

//     pub fn submit(self: *JobGraph, job: *Job) void {
//         if (job.unmet_dependencies.load(.acquire) == 0) {
//             self.ready_queue.push(job);
//         }
//     }
// };
