cmake_minimum_required(VERSION 3.0.0)

cmake_policy(SET CMP0015 NEW)

add_library(DXFC_Snapshot OBJECT Snapshot.cpp SnapshotManager.cpp)
