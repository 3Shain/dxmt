#import <Foundation/Foundation.h>
#include "sqlite3.h"
#define WINEMETAL_API
#include "../winemetal_thunks.h"

@interface CacheReader : NSObject
- (instancetype)initWithPath:(NSString *)path version:(uint64_t)version;
- (dispatch_data_t)get:(NSData *)key;
@end

@interface CacheWriter : NSObject
- (instancetype)initWithPath:(NSString *)path version:(uint64_t)version;
- (void)set:(NSData *)key value:(dispatch_data_t)value;
@end

@interface CacheReader () {
  sqlite3 *_db;
  sqlite3_stmt *_stmt;
}
@end

static inline NSString *
resolve_cache_dir(NSString *path, bool path_is_file) {
  if (![path hasPrefix:@"/"]) {
    char buf[PATH_MAX];
    size_t len = confstr(_CS_DARWIN_USER_CACHE_DIR, buf, sizeof(buf));

    if (!len) {
      return nil;
    }

    NSString *base = [NSString stringWithUTF8String:buf];
    path = [base stringByAppendingPathComponent:path];
  }
  [[NSFileManager defaultManager] createDirectoryAtPath:path_is_file ? [path stringByDeletingLastPathComponent] : path
                            withIntermediateDirectories:YES
                                             attributes:nil
                                                  error:nil];
  return path;
}

@implementation CacheReader

- (instancetype)initWithPath:(NSString *)path version:(uint64_t)version {
  if ((self = [super init])) {
    NSString *dbPath = resolve_cache_dir(path, true);
    if (!dbPath) {
      NSLog(@"[CacheReader] Failed to resolve cache path");
      return nil;
    }
    NSString *tableName = [NSString stringWithFormat:@"cache_%llu", version];
    if (sqlite3_open_v2([dbPath fileSystemRepresentation], &_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL) !=
        SQLITE_OK) {
      NSLog(@"[CacheReader] Failed to open DB: %s", sqlite3_errmsg(_db));
      sqlite3_close(_db);
      return nil;
    }

    NSString *sqlGet = [NSString stringWithFormat:@"SELECT value FROM %@ WHERE key = ?;", tableName];
    if (sqlite3_prepare_v2(_db, sqlGet.UTF8String, -1, &_stmt, NULL) != SQLITE_OK) {
      NSLog(@"[CacheReader] Failed to prepare SELECT: %s", sqlite3_errmsg(_db));
      sqlite3_close(_db);
      return nil;
    }
  }
  return self;
}

- (dispatch_data_t)get:(NSData *)key {
  sqlite3_reset(_stmt);
  sqlite3_clear_bindings(_stmt);
  sqlite3_bind_blob64(_stmt, 1, key.bytes, key.length, SQLITE_STATIC);

  dispatch_data_t result = nil;
  if (sqlite3_step(_stmt) == SQLITE_ROW) {
    const void *bytes = sqlite3_column_blob(_stmt, 0);
    int len = sqlite3_column_bytes(_stmt, 0);
    result = dispatch_data_create(bytes, len, nil, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
  }
  sqlite3_reset(_stmt);
  return result;
}

- (void)dealloc {
  if (_stmt)
    sqlite3_finalize(_stmt);
  if (_db)
    sqlite3_close(_db);
  [super dealloc];
}

@end

@interface CacheWriter () {
  sqlite3 *_db;
  sqlite3_stmt *_stmt;
}
@end

@implementation CacheWriter

- (instancetype)initWithPath:(NSString *)path version:(uint64_t)version {
  if ((self = [super init])) {
    NSString *dbPath = resolve_cache_dir(path, true);
    if (!dbPath) {
      NSLog(@"[CacheReader] Failed to resolve cache path");
      return nil;
    }

    NSString *lockPath = [dbPath stringByAppendingString:@"-lock"];
    int fd = open([lockPath fileSystemRepresentation], O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
      NSLog(@"[CacheWriter] Failed to open file for locking %@", lockPath);
      return nil;
    }
    flock(fd, LOCK_EX);

    if (sqlite3_open_v2(
            [dbPath fileSystemRepresentation], &_db, //
            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX, NULL
        ) != SQLITE_OK) {
      NSLog(@"[CacheWriter] Failed to open DB: %s", sqlite3_errmsg(_db));
      flock(fd, LOCK_UN);
      close(fd);
      return nil;
    }

    sqlite3_exec(_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    sqlite3_exec(_db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    NSString *tableName = [NSString stringWithFormat:@"cache_%llu", version];
    NSString *sqlCreate = [NSString stringWithFormat:@"CREATE TABLE IF NOT EXISTS %@ ("
                                                      "key BLOB PRIMARY KEY, "
                                                      "value BLOB NOT NULL);",
                                                     tableName];

    char *errMsg = NULL;
    if (sqlite3_exec(_db, sqlCreate.UTF8String, NULL, NULL, &errMsg) != SQLITE_OK) {
      NSLog(@"[CacheWriter] Failed to create table: %s", errMsg);
      sqlite3_free(errMsg);
    }

    flock(fd, LOCK_UN);
    close(fd);

    NSString *sqlSet = [NSString stringWithFormat:@"INSERT OR REPLACE INTO %@ (key, value) VALUES (?, ?);", tableName];
    if (sqlite3_prepare_v2(_db, sqlSet.UTF8String, -1, &_stmt, NULL) != SQLITE_OK) {
      NSLog(@"[CacheWriter] Failed to prepare INSERT: %s", sqlite3_errmsg(_db));
      sqlite3_close(_db);
      return nil;
    }
  }
  return self;
}

- (void)set:(NSData *)key value:(dispatch_data_t)value {
  sqlite3_reset(_stmt);
  sqlite3_clear_bindings(_stmt);
  sqlite3_bind_blob64(_stmt, 1, key.bytes, key.length, SQLITE_STATIC);

  const void *bytes = NULL;
  size_t length = 0;
  dispatch_data_t flat = dispatch_data_create_map(value, &bytes, &length);
  sqlite3_bind_blob64(_stmt, 2, bytes, length, SQLITE_STATIC);

  if (sqlite3_step(_stmt) != SQLITE_DONE) {
    NSLog(@"[CacheWriter] Failed to insert: %s", sqlite3_errmsg(_db));
  }

  dispatch_release(flat);
  sqlite3_reset(_stmt);
}

- (void)dealloc {
  if (_stmt)
    sqlite3_finalize(_stmt);
  if (_db)
    sqlite3_close(_db);
  [super dealloc];
}

@end

int
_CacheReader_alloc_init(void *obj) {
  struct unixcall_cache_alloc_init *params = obj;
  NSString *path = [[NSString alloc] initWithCString:params->path.ptr encoding:NSUTF8StringEncoding];
  params->ret_cache = (obj_handle_t)[[CacheReader alloc] initWithPath:path version:params->version];
  [path release];
  return 0;
}

int
_CacheReader_get(void *obj) {
  struct unixcall_cache_get *params = obj;
  NSData *key =
      [[NSData alloc] initWithBytesNoCopy:(void *)params->key.ptr length:params->key_length freeWhenDone:false];
  CacheReader *reader = (CacheReader *)params->cache;
  params->ret_data = (obj_handle_t)[reader get:key];
  [key release];
  return 0;
}

int
_CacheWriter_alloc_init(void *obj) {
  struct unixcall_cache_alloc_init *params = obj;
  NSString *path = [[NSString alloc] initWithCString:params->path.ptr encoding:NSUTF8StringEncoding];
  params->ret_cache = (obj_handle_t)[[CacheWriter alloc] initWithPath:path version:params->version];
  [path release];
  return 0;
}

int
_CacheWriter_set(void *obj) {
  struct unixcall_cache_set *params = obj;
  NSData *key =
      [[NSData alloc] initWithBytesNoCopy:(void *)params->key.ptr length:params->key_length freeWhenDone:false];
  CacheWriter *writer = (CacheWriter *)params->cache;
  [writer set:key value:(dispatch_data_t)params->value_data];
  [key release];
  return 0;
}
