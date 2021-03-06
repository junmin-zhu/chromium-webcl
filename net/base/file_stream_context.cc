// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/file_stream_context.h"

#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "net/base/file_stream_net_log_parameters.h"
#include "net/base/net_errors.h"

#if defined(OS_ANDROID)
#include "base/android/content_uri_utils.h"
#endif

namespace {

void CallInt64ToInt(const net::CompletionCallback& callback, int64 result) {
  callback.Run(static_cast<int>(result));
}

}

namespace net {

FileStream::Context::IOResult::IOResult()
    : result(OK),
      os_error(0) {
}

FileStream::Context::IOResult::IOResult(int64 result, int os_error)
    : result(result),
      os_error(os_error) {
}

// static
FileStream::Context::IOResult FileStream::Context::IOResult::FromOSError(
    int64 os_error) {
  return IOResult(MapSystemError(os_error), os_error);
}

// ---------------------------------------------------------------------

FileStream::Context::OpenResult::OpenResult() {
}

FileStream::Context::OpenResult::OpenResult(base::File file,
                                            IOResult error_code)
    : file(file.Pass()),
      error_code(error_code) {
}

FileStream::Context::OpenResult::OpenResult(RValue other)
    : file(other.object->file.Pass()),
      error_code(other.object->error_code) {
}

FileStream::Context::OpenResult& FileStream::Context::OpenResult::operator=(
    RValue other) {
  if (this != other.object) {
    file = other.object->file.Pass();
    error_code = other.object->error_code;
  }
  return *this;
}

// ---------------------------------------------------------------------

void FileStream::Context::Orphan() {
  DCHECK(!orphaned_);

  orphaned_ = true;
  if (file_.IsValid())
    bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_OPEN);

  if (!async_in_progress_) {
    CloseAndDelete();
  } else if (file_.IsValid()) {
    CancelIo(file_.GetPlatformFile());
  }
}

void FileStream::Context::OpenAsync(const base::FilePath& path,
                                    int open_flags,
                                    const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);
  BeginOpenEvent(path);

  bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &Context::OpenFileImpl, base::Unretained(this), path, open_flags),
      base::Bind(&Context::OnOpenCompleted, base::Unretained(this), callback));
  DCHECK(posted);

  async_in_progress_ = true;

  // TODO(rvargas): Figure out what to do here. For POSIX, async IO is
  // implemented by doing blocking IO on another thread, so file_ is not really
  // async, but this code has sync and async paths so it has random checks to
  // figure out what mode to use. We should probably make this class async only,
  // and make consumers of sync IO use base::File.
  async_ = true;
}

int FileStream::Context::OpenSync(const base::FilePath& path, int open_flags) {
  DCHECK(!async_in_progress_);

  BeginOpenEvent(path);
  OpenResult result = OpenFileImpl(path, open_flags);
  if (result.file.IsValid()) {
    file_ = result.file.Pass();
    // TODO(satorux): Remove this once all async clients are migrated to use
    // Open(). crbug.com/114783
    if (open_flags & base::File::FLAG_ASYNC)
      OnAsyncFileOpened();
  } else {
    ProcessOpenError(result.error_code);
  }
  return result.error_code.result;
}

void FileStream::Context::CloseSync() {
  DCHECK(!async_in_progress_);
  if (file_.IsValid()) {
    file_.Close();
    bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_OPEN);
  }
}

void FileStream::Context::CloseAsync(const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);
  bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&Context::CloseFileImpl, base::Unretained(this)),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this),
                 IntToInt64(callback),
                 FILE_ERROR_SOURCE_CLOSE));
  DCHECK(posted);

  async_in_progress_ = true;
}

void FileStream::Context::SeekAsync(Whence whence,
                                    int64 offset,
                                    const Int64CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(
          &Context::SeekFileImpl, base::Unretained(this), whence, offset),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this),
                 callback,
                 FILE_ERROR_SOURCE_SEEK));
  DCHECK(posted);

  async_in_progress_ = true;
}

int64 FileStream::Context::SeekSync(Whence whence, int64 offset) {
  IOResult result = SeekFileImpl(whence, offset);
  RecordError(result, FILE_ERROR_SOURCE_SEEK);
  return result.result;
}

void FileStream::Context::FlushAsync(const CompletionCallback& callback) {
  DCHECK(!async_in_progress_);

  bool posted = base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&Context::FlushFileImpl, base::Unretained(this)),
      base::Bind(&Context::ProcessAsyncResult,
                 base::Unretained(this),
                 IntToInt64(callback),
                 FILE_ERROR_SOURCE_FLUSH));
  DCHECK(posted);

  async_in_progress_ = true;
}

int FileStream::Context::FlushSync() {
  IOResult result = FlushFileImpl();
  RecordError(result, FILE_ERROR_SOURCE_FLUSH);
  return result.result;
}

void FileStream::Context::RecordError(const IOResult& result,
                                      FileErrorSource source) const {
  if (result.result >= 0) {
    // |result| is not an error.
    return;
  }

  if (!orphaned_) {
    bound_net_log_.AddEvent(
        NetLog::TYPE_FILE_STREAM_ERROR,
        base::Bind(&NetLogFileStreamErrorCallback,
                   source, result.os_error,
                   static_cast<net::Error>(result.result)));
  }

  RecordFileError(result.os_error, source, record_uma_);
}

void FileStream::Context::BeginOpenEvent(const base::FilePath& path) {
  std::string file_name = path.AsUTF8Unsafe();
  bound_net_log_.BeginEvent(NetLog::TYPE_FILE_STREAM_OPEN,
                            NetLog::StringCallback("file_name", &file_name));
}

FileStream::Context::OpenResult FileStream::Context::OpenFileImpl(
    const base::FilePath& path, int open_flags) {
#if defined(OS_POSIX)
  // Always use blocking IO.
  open_flags &= ~base::File::FLAG_ASYNC;
#endif
  base::File file;
#if defined(OS_ANDROID)
  if (path.IsContentUri()) {
    // Check that only Read flags are set.
    DCHECK_EQ(open_flags & ~base::File::FLAG_ASYNC,
              base::File::FLAG_OPEN | base::File::FLAG_READ);
    file = base::OpenContentUriForRead(path);
  } else {
#endif  // defined(OS_ANDROID)
    // FileStream::Context actually closes the file asynchronously,
    // independently from FileStream's destructor. It can cause problems for
    // users wanting to delete the file right after FileStream deletion. Thus
    // we are always adding SHARE_DELETE flag to accommodate such use case.
    // TODO(rvargas): This sounds like a bug, as deleting the file would
    // presumably happen on the wrong thread. There should be an async delete.
    open_flags |= base::File::FLAG_SHARE_DELETE;
    file.Initialize(path, open_flags);
#if defined(OS_ANDROID)
  }
#endif  // defined(OS_ANDROID)
  if (!file.IsValid())
    return OpenResult(base::File(), IOResult::FromOSError(GetLastErrno()));

  return OpenResult(file.Pass(), IOResult(OK, 0));
}

FileStream::Context::IOResult FileStream::Context::CloseFileImpl() {
  file_.Close();
  return IOResult(OK, 0);
}

void FileStream::Context::ProcessOpenError(const IOResult& error_code) {
  bound_net_log_.EndEvent(NetLog::TYPE_FILE_STREAM_OPEN);
  RecordError(error_code, FILE_ERROR_SOURCE_OPEN);
}

void FileStream::Context::OnOpenCompleted(const CompletionCallback& callback,
                                          OpenResult open_result) {
  if (!open_result.file.IsValid()) {
    ProcessOpenError(open_result.error_code);
  } else if (!orphaned_) {
    file_ = open_result.file.Pass();
    OnAsyncFileOpened();
  }
  OnAsyncCompleted(IntToInt64(callback), open_result.error_code.result);
}

void FileStream::Context::CloseAndDelete() {
  DCHECK(!async_in_progress_);

  if (file_.IsValid()) {
    bool posted = task_runner_.get()->PostTaskAndReply(
        FROM_HERE,
        base::Bind(base::IgnoreResult(&Context::CloseFileImpl),
                   base::Unretained(this)),
        base::Bind(&Context::OnCloseCompleted, base::Unretained(this)));
    DCHECK(posted);
  } else {
    delete this;
  }
}

void FileStream::Context::OnCloseCompleted() {
  delete this;
}

Int64CompletionCallback FileStream::Context::IntToInt64(
    const CompletionCallback& callback) {
  return base::Bind(&CallInt64ToInt, callback);
}

void FileStream::Context::ProcessAsyncResult(
    const Int64CompletionCallback& callback,
    FileErrorSource source,
    const IOResult& result) {
  RecordError(result, source);
  OnAsyncCompleted(callback, result.result);
}

void FileStream::Context::OnAsyncCompleted(
    const Int64CompletionCallback& callback,
    int64 result) {
  // Reset this before Run() as Run() may issue a new async operation. Also it
  // should be reset before CloseAsync() because it shouldn't run if any async
  // operation is in progress.
  async_in_progress_ = false;
  if (orphaned_)
    CloseAndDelete();
  else
    callback.Run(result);
}

}  // namespace net

