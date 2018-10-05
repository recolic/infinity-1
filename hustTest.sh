#!/bin/bash

/home/rdma_match/tmp/infinity/release/RdmaNativeTest &
ssh inode112 /home/rdma_match/tmp/infinity/release/RdmaNativeTest inode111/10.10.0.111:12321
echo Sleep 6 to wait for the server shutdown
sleep 6
