#!/usr/bin/env bash

test_via_curl() {
  URL=http://localhost:8888
  echo $URL

  # GET all buckets - shouldn't have any.
  curl -i $URL/

  # GET key - should return 404
  curl -i $URL/746bkt/p2key

  # PUT bucket. Should succeed.
  curl -i -X PUT $URL/746bkt/

  # GET bucket. Should succeed.
  curl -i $URL/746bkt/

  # PUT key. Should succeed.
  curl -i -X PUT $URL/746bkt/key1 -d "cloudfs"
  curl -i -X PUT $URL/746bkt/p2key -d "cloudfs"

  # GET key. Should succeed.
  curl -i $URL/746bkt/p2key

  # GET bucket. Should show key.
  curl -i $URL/746bkt/

  # GET all buckets. Should only show bucket, not key.
  curl -i $URL/

  # DELETE bucket. Should fail since its not empty.
  curl -i -X DELETE $URL/746bkt/

  # DELETE key. Should succeed.
  curl -i -X DELETE $URL/746bkt/p2key

  # DELETE bucket. Should succeed.
  curl -i -X DELETE $URL/746bkt/

  # GET all buckets. Shouldn't have any.
  curl -i $URL/
}

test_libs3_bin() {
  unset http_proxy
  unset https_proxy

  BUILD_DIR=/users/ankushj/repos/746/libs3/build
  BIN=$BUILD_DIR/bin/s3

  export LD_LIBRARY_PATH=$BUILD_DIR/lib
  export S3_ACCESS_KEY_ID=whatever
  export S3_SECRET_ACCESS_KEY=whatever
  export S3_HOSTNAME=localhost:8888

  $BIN -u list # should show nothing
  $BIN -u create 746bkt # should work
  $BIN -u list # should show bucket
  $BIN -u create 746bkt # should fail - bucket exists
  $BIN -us delete 746bkt # should work
  $BIN -us delete 746bkt # should fail now
  $BIN -u create 746bkt # should work

  echo "cloudfs" > p2key.data
  $BIN -u put 746bkt/p2key filename=p2key.data # should work
  rm -f p2key.data

  $BIN -u get 746bkt/p2key # should work
  $BIN -u delete 746bkt/p2key # should work
  $BIN -u get 746bkt/p2key # should NOT work

  $BIN -u delete 746bkt # should work
  $BIN -u delete 746bkt # should NOT work
}
