/***************************************************************
    Copyright 2016, 2017 Defence Science and Technology Group,
    Department of Defence,
    Australian Government

	This file is part of LASAGNE.

    LASAGNE is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as
    published by the Free Software Foundation, either version 3
    of the License, or (at your option) any later version.

    LASAGNE is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with LASAGNE.  If not, see <http://www.gnu.org/licenses/>.
***************************************************************/
#define TTY_READSTREAM_CPP

#include "TTY_ReadStream.h"

// This only works on Win32 platforms and on Unix platforms supporting POSIX aio calls.
#if defined (ACE_HAS_WIN32_OVERLAPPED_IO) || defined (ACE_HAS_AIO_CALLS)

#include "ace/Min_Max.h"
#include "ace/Guard_T.h"
#include "ace/Message_Block.h"

namespace DEV
{
    TTY_ReadStream::TTY_ReadStream(size_t size) : TTY_Stream(size)
    {
    }

    TTY_ReadStream::~TTY_ReadStream(void)
    {
        this->cancel();
    }

    int
    TTY_ReadStream::open_stream(ACE_Handler &handler)
    {
        if (this->open(handler) == 0) {
            ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, mon, *this, -1); return this->signal_reader();
        }
        return -1;
    }

    int
    TTY_ReadStream::close_stream(bool cancel_io)
    {
        if (cancel_io) { this->cancel(); } return TTY_Stream::close_stream();
    }

    ssize_t
    TTY_ReadStream::read_data(char iov_buf[], int iov_len, int timeout)
    {
        int recv_len = 0;

        if (iov_buf && iov_len > 0) {
            ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, this_mon, this->lock_, 0);         // Only One Writer at a time

            ACE_Time_Value tv(DAF_OS::gettimeofday(timeout));

            char * iov_ptr = iov_buf;

            for (size_t io_len = 0; iov_len > 0;) {
                if (this->is_active()) {
                    ACE_GUARD_RETURN(ACE_SYNCH_MUTEX, stream_mon, *this, recv_len); // Only One Writer at a time
                    while (this->is_active() && this->is_empty()) {
                        this->signal_reader();                              // Ensure Writer is Signalled
                        if (this->b_cond_.wait(timeout ? &tv : 0) == -1) {      // Wait on Full Condition
                            if (this->is_empty()) return recv_len;
                        }
                    }

                    if (this->is_active() && (io_len = this->get_bump(this->get(iov_ptr, iov_len))) != 0) {
                        this->signal_reader();
                    } else break;
                } else break; // Not Active

                iov_len     -= int(io_len);
                iov_ptr     += int(io_len);
                recv_len    += int(io_len);

                DAF_OS::thr_yield();  // Yield a little for IO to Start
            }
        }

        return this->is_active() ? recv_len : -1;
    }

    ssize_t
    TTY_ReadStream::peek_data(char iov_buf[], int iov_len)
    {
        return this->get(iov_buf, iov_len);  /// Note Not Locked AT ALL!!!
    }

    ssize_t
    TTY_ReadStream::recv_stream(ACE_Message_Block &mb)
    {
        return this->read(mb, mb.space());
    }
} // namespace DEV

// This only works on Win32 platforms and on Unix platforms supporting POSIX aio calls.
#endif // defined (ACE_HAS_WIN32_OVERLAPPED_IO) || defined (ACE_HAS_AIO_CALLS)
