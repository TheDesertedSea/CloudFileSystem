/**
 * @file snapshot.h
 * @brief Snapshot controller
 * @author Cundao Yu <cundaoy@andrew.cmu.edu>
 */
#pragma once

#include <memory>
#include <vector>

#include "util.h"
#include "cloudfs.h"
#include "cloudfs_controller.h"

/**
 * Snapshot controller
 */
class SnapshotController
{
    struct cloudfs_state *state_;                           // cloudfs state
    std::shared_ptr<DebugLogger> logger_;                   // logger
    std::shared_ptr<CloudfsController> cloudfs_controller_; // cloudfs controller
    std::string snapshot_stub_path_;                        // absolute path to the ".snapshot" file

public:
    SnapshotController(struct cloudfs_state *state, std::shared_ptr<DebugLogger> logger, std::shared_ptr<CloudfsController> cloudfs_controller);
    ~SnapshotController();

    /**
     * Create a snapshot
     * @param timestamp The timestamp of the snapshot
     * @return 0 on success, negative errno on failure
     */
    int create_snapshot(unsigned long *timestamp);

    /**
     * Restore a snapshot
     * @param timestamp The timestamp of the snapshot
     * @return 0 on success, negative errno on failure
     */
    int restore_snapshot(unsigned long *timestamp);

    /**
     * List snapshots
     * @param snapshot_list The list of snapshots, used as output
     * @return 0 on success, negative errno on failure
     */
    int list_snapshots(unsigned long *snapshot_list);

    /**
     * Delete a snapshot
     * @param timestamp The timestamp of the snapshot
     * @return 0 on success, negative errno on failure
     */
    int delete_snapshot(unsigned long *timestamp);

    /**
     * Install a snapshot
     * @param timestamp The timestamp of the snapshot
     * @return 0 on success, negative errno on failure
     */
    int install_snapshot(unsigned long *timestamp);

    /**
     * Uninstall a snapshot
     * @param timestamp The timestamp of the snapshot
     * @return 0 on success, negative errno on failure
     */
    int uninstall_snapshot(unsigned long *timestamp);

    /**
     * Persist the snapshot info to the cloud
     */
    void persist();

private:
    /**
     * Get the snapshot count
     * @param count The snapshot count, used as output
     * @return 0 on success, negative errno on failure
     */
    int get_snapshot_count(int &count);

    /**
     * Set the snapshot count
     * @param count The snapshot count
     * @return 0 on success, negative errno on failure
     */
    int set_snapshot_count(int count);

    /**
     * Get the snapshot list
     * @param list The snapshot list, used as output
     * @return 0 on success, negative errno on failure
     */
    int get_snapshot_list(std::vector<unsigned long> &list);

    /**
     * Set the snapshot list
     * @param list The snapshot list
     * @return 0 on success, negative errno on failure
     */
    int set_snapshot_list(std::vector<unsigned long> &list);

    /**
     * Get the installed snapshot count
     * @param count The installed snapshot count, used as output
     * @return 0 on success, negative errno on failure
     */
    int get_installed_snapshot_count(int &count);

    /**
     * Set the installed snapshot count
     * @param count The installed snapshot count
     * @return 0 on success, negative errno on failure
     */
    int set_installed_snapshot_count(int count);

    /**
     * Get the installed snapshot list
     * @param list The installed snapshot list, used as output
     * @return 0 on success, negative errno on failure
     */
    int get_installed_snapshot_list(std::vector<unsigned long> &list);

    /**
     * Set the installed snapshot list
     * @param list The installed snapshot list
     * @return 0 on success, negative errno on failure
     */
    int set_installed_snapshot_list(std::vector<unsigned long> &list);

    /**
     * Clear user contents under a directory
     * @param path The path to the directory
     * @return 0 on success, negative errno on failure
     */
    int clear_dir(const std::string &path);
};