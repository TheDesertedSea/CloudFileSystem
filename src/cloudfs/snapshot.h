#pragma once
#include <memory>
#include <vector>
#include "util.h"
#include "cloudfs.h"
#include "cloudfs_controller.h"

class SnapshotController {
    struct cloudfs_state* state_;
    std::shared_ptr<DebugLogger> logger_;
    std::shared_ptr<CloudfsController> cloudfs_controller_;
    std::string snapshot_stub_path_;

public:
    SnapshotController(struct cloudfs_state* state, std::shared_ptr<DebugLogger> logger, std::shared_ptr<CloudfsController> cloudfs_controller);
    ~SnapshotController();

    int create_snapshot(unsigned long* timestamp);
    int restore_snapshot(unsigned long* timestamp);
    int list_snapshots(unsigned long* current_ts);
    int delete_snapshot(unsigned long* timestamp);
    int install_snapshot(unsigned long* timestamp);
    int uninstall_snapshot(unsigned long* timestamp);

private:
    int get_snapshot_count(int& count);
    int set_snapshot_count(int count);
    int get_snapshot_list(std::vector<unsigned long>& list);
    int set_snapshot_list(std::vector<unsigned long>& list);
    int get_installed_snapshot_count(int& count);
    int set_installed_snapshot_count(int count);
    int get_installed_snapshot_list(std::vector<unsigned long>& list);
    int set_installed_snapshot_list(std::vector<unsigned long>& list);
};