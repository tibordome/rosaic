// *****************************************************************************
//
// © Copyright 2020, Septentrio NV/SA.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//    1. Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//    2. Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//    3. Neither the name of the copyright holder nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE 
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE. 
//
// *****************************************************************************

// *****************************************************************************
//
// Boost Software License - Version 1.0 - August 17th, 2003
// 
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:

// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// *****************************************************************************

#include <boost/thread.hpp>
// Boost's thread enables the use of multiple threads of execution with shared data in portable C++ code. It provides classes and functions for managing the threads themselves, along with others for synchronizing data between the threads or providing separate copies of data specific to individual threads. 
#include <boost/thread/condition.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/system/error_code.hpp>

#ifndef ASYNC_MANAGER_HPP
#define ASYNC_MANAGER_HPP

/**
 * @file async_manager.hpp
 * @date 20/08/20
 * @brief Implements asynchronous operations for an I/O manager such as reading NMEA messages or SBF blocks and sending commands to serial port or via TCP/IP
 */
 
namespace io_comm_mosaic 
{
	/**
	 * @class Manager
	 * @brief Interface (in C++ terms), that could be used for any I/O manager, synchronous and asynchronous alike
	 */
	class Manager {
		public:
			typedef boost::function<void(uint8_t*, std::size_t&)> Callback;
			virtual ~Manager() {}
			virtual void setCallback(const Callback& callback) = 0;
		 
			// virtual void setRawDataCallback(const Callback& callback) = 0; //later for MeasEpoch3, if we get that far..
		 
			// virtual bool send(const uint8_t* data, const unsigned int size) = 0; // for sending data to Rx
		   
			virtual void wait(const boost::posix_time::time_duration& timeout) = 0;
		 
			virtual bool isOpen() const = 0;
	};


	/**
	 * @class AsyncManager
	 * @brief This is the central interface between this ROS driver and the mosaic receiver(s), managing I/O operations such as reading messages and sending commands..
	 * 
	 * StreamT is either boost::asio::serial_port or boost::asio::tcp::ip
	 */
	template <typename StreamT>
	class AsyncManager : public Manager 
	{
		public:
			/**
			 * @brief Multithreaded programs use mutexes for synchronization.
			 * 
			 * Boost.Thread provides different mutex classes with boost::mutex being the simplest. The basic principle of a mutex is to prevent other threads 
			 * from taking ownership while a particular thread owns (by calling lock()) the mutex. Once released, a different thread can take ownership. 
			 * This causes threads to wait until the thread that owns the mutex has finished processing and releases its ownership of the mutex.
			 * E.g. Because std::cout is a global object shared by the threads, access must be synchronized.
			 * Helps to avoid data races (A data race occurs when all: a) two or more threads in a single process access the same memory location concurrently, and b) at least one of the accesses is for writing, and c)the threads are not using any exclusive locks to control their accesses to that memory.) and undefined behavior
			 */	
			typedef boost::mutex Mutex;
			/**
			 * @brief scoped_lock is meant to carry out the tasks for locking, unlocking, try-locking and timed-locking (recursive or not) for the mutex
			 * 
			 * It is more robust than raw mutex, e.g. with traditional mutex, an exception may occur while your mutex is locked, and your call to unlock() may never be reached, even though you do not have any return statement between your call to lock() and your call to unlock().
			 */
			typedef boost::mutex::scoped_lock ScopedLock;
			
			/**
			 * @brief Class constructor
			 * @param stream Whether TCP/IP or serial communication, either boost::asio::serial_port or boost::asio::tcp::ip
			 * @param io_service The io_context object. The io_context represents your program's link to the operating system's I/O services. 
			 */
			AsyncManager(boost::shared_ptr<StreamT> stream,
				   boost::shared_ptr<boost::asio::io_service> io_service,
				   std::size_t buffer_size = 8192);
			virtual ~AsyncManager();
	 
			void setCallback(const Callback& callback) { read_callback_ = callback; }
	 
			// void setRawDataCallback(const Callback& callback) { write_callback_ = callback; }
	 
			// bool send(const uint8_t* data, const unsigned int size);
			void wait(const boost::posix_time::time_duration& timeout);
	 
			bool isOpen() const { return stream_->is_open(); }
	 
		protected:
			
			//! Reads in via async_read_some and hands certain number of bytes (bytes_transferred) over to async_read_some_handler 
			void doRead();
			
			//!  Handler for async_read_some (Boost library)..
			void async_read_some_handler(const boost::system::error_code&, std::size_t);
	 
			// void doWrite();
	 
			//! Closes Stream "stream_"
			void doClose();
			
			//! Stream, represents either serial or TCP/IP connection
			boost::shared_ptr<StreamT> stream_; 
			//! io_context object
			boost::shared_ptr<boost::asio::io_service> io_service_; 
			//! As name suggest, the read mutex
			Mutex read_mutex_; 
			
			/**
			 * @brief A condition object is always used in conjunction with a mutex object (an object whose type is a model of a Mutex or one of its 
			 * refinements). 
			 * 
			 * The mutex object must be locked prior to waiting on the condition, which is verified by passing a lock object 
			 * (an object whose type is a model of Lock or one of its refinements) to the condition object's wait functions. Upon blocking on the 
			 * condition object, the thread unlocks the mutex object. 
			 */
			boost::condition read_condition_;
			std::vector<uint8_t> in_; 
			//! Keeps track of how large the buffer (not just space allocated) is at the moment
			std::size_t in_buffer_size_;  
			// Mutex write_mutex_; 
			// boost::condition write_condition_;
			std::vector<uint8_t> out_; 
	 
			boost::shared_ptr<boost::thread> callback_thread_; 
			Callback read_callback_; 
			// Callback write_callback_; 
	 
			bool stopping_; 
	};
	 
	template <typename StreamT>
	AsyncManager<StreamT>::AsyncManager(boost::shared_ptr<StreamT> stream,
			 boost::shared_ptr<boost::asio::io_service> io_service,
			 std::size_t buffer_size) : stopping_(false) // Since buffer_size = 8912 in declaration, no need in definition any more (even yields error message, since "overwrite").
	{
		ROS_DEBUG("Setting the stream private variable of the AsyncManager class.");
		stream_ = stream;
		io_service_ = io_service;
		in_.resize(buffer_size);
		in_buffer_size_ = 0;

		out_.reserve(buffer_size); 	// Note that std::vector::reserve() requests to reserve vector capacity be at least enough to contain n elements. 
									// Reallocation happens if there is need of even more space.
		 
		io_service_->post(boost::bind(&AsyncManager<StreamT>::doRead, this));
		// This function is used to ask the io_service to execute the given handler, but without allowing the io_service to call the handler from inside this function.
		// The function signature of the handler must be: void handler(); 
		// The io_service guarantees that the handler (given as parameter) will only be called in a thread in which the run(), run_one(), poll() or poll_one() member functions is currently being invoked. 
		// So the fundamental difference is that dispatch will execute the work right away if it can and queue it otherwise while post queues the work no matter what.
		callback_thread_.reset(new boost::thread(boost::bind(&boost::asio::io_service::run, io_service_))); // io_service_ is already pointer
		// If the value of the pointer for the current thread is changed using reset(), then the previous value is destroyed by calling the cleanup routine. Alternatively, the stored value can be reset to NULL and the prior value returned by calling the release() member function, allowing the application to take back responsibility for destroying the object. 
	}
	 
	template <typename StreamT>
	AsyncManager<StreamT>::~AsyncManager() 
	{
		io_service_->post(boost::bind(&AsyncManager<StreamT>::doClose, this));
		callback_thread_->join(); 
		//io_service_->reset(); 
		// Reset the io_service in preparation for a subsequent run() invocation. 
		// must be called prior to any second or later set of invocations of the run(), run_one() etc.
		// After a call to reset(), the io_service object's stopped() function will return false. (true only after true stop)
	}
	 
	/*
	template <typename StreamT>
	bool AsyncManager<StreamT>::send(const uint8_t* data,
									 const unsigned int size) {
		ScopedLock lock(write_mutex_);
		if(size == 0) {
			ROS_ERROR("mosaic-X5 AsyncManager::send: Size of message to send is 0");
			return true;
		}
	 
		if (out_.capacity() - out_.size() < size) {// Returns the size of the storage space currently allocated for the vector, expressed in terms of elements.
		// size vs capacity of vector: Size: the number of items currently in the vector Capacity: how many items can be fit in the vector before it is "full". Once full, adding new items will result in a new, larger block of memory being allocated and the existing items being copied to it

			ROS_ERROR("mosaic-X5 AsyncManager::send: Output buffer too full to send message");
			return false;
		}
		out_.insert(out_.end(), data, data + size);
		// vector_name.insert (position, val) vs vector_name.insert(position, size, val) vs vector_name.insert(position, iterator1, iterator2)
		// all three return an iterator which points to the newly inserted element. 
	 
		io_service_->post(boost::bind(&AsyncManager<StreamT>::doWrite, this));
		return true;
	}
	 
	template <typename StreamT>
	void AsyncManager<StreamT>::doWrite() {
		ScopedLock lock(write_mutex_);
		// Do nothing if out buffer is empty
		if (out_.size() == 0) {
			return;
		}
		// Write all the data in the out buffer
		boost::asio::write(*stream_, boost::asio::buffer(out_.data(), out_.size()));
		//  The boost::asio::buffer function is used to create a buffer object to represent raw memory, an array of POD elements, a vector of POD elements, or a std::string. 
		// POD stands for Plain Old Data - that is, a class (whether defined with the keyword struct or the keyword class) without constructors, destructors and virtual members functions.
		// A buffer object represents a contiguous region of memory as a 2-tuple consisting of a pointer and size in bytes. 
		// A tuple of the form {void*, size_t} specifies a mutable (modifiable) region of memory. Similarly, a tuple of the form {const void*, size_t} specifies a const (non-modifiable) region of memory. These two forms [typedef std::pair<void*, std::size_t> mutable_buffer; typedef std::pair<const void*, std::size_t> const_buffer;] correspond to the classes mutable_buffer and const_buffer, respectively. To mirror C++'s conversion rules, a mutable_buffer is implicitly convertible to a const_buffer, and the opposite conversion is not permitted. 
		// Also, An individual buffer may be created from a builtin array, std::vector, std::array or !boost::array of POD elements!, char d1[128]; size_t bytes_transferred = sock.receive(boost::asio::buffer(d1));
		// write(SyncWriteStream& s, const ConstBufferSequence& buffers,    typename enable_if<is_const_buffer_sequence<ConstBufferSequence>::value>::type* = 0), so stream needs to satisfy certain rules
	 
		if (debug >= 2) {
			// Print the data that was sent
			std::ostringstream oss;
			for (std::vector<uint8_t>::iterator it = out_.begin();
				it != out_.end(); ++it)
					oss << boost::format("%02x") % static_cast<unsigned int>(*it) << " ";
			ROS_DEBUG("mosaic-X5 sent %li bytes: \n%s", out_.size(), oss.str().c_str());
		}
		// Clear the buffer & unlock
		out_.clear(); // clear() function is used to remove all the elements of the vector container, thus making it size 0.
		write_condition_.notify_all(); // Change the state of all threads waiting on *this to ready. If there are no waiting threads, notify_all() has no effect.
	}
	*/


	template <typename StreamT>
	void AsyncManager<StreamT>::doRead() {
		ROS_DEBUG("Entered doRead() method of the AsyncManager class.");
		ScopedLock lock(read_mutex_);
		stream_->async_read_some(
		   boost::asio::buffer(in_.data() + in_buffer_size_,
							   in_.size() - in_buffer_size_),
							   boost::bind(&AsyncManager<StreamT>::async_read_some_handler, this,
								   boost::asio::placeholders::error,
								   boost::asio::placeholders::bytes_transferred));
									// handler is async_read_some_handler!!, call postponed as with post..
		//ROS_DEBUG("After async_read_some command.");
	}
	 
	template <typename StreamT>
	void AsyncManager<StreamT>::async_read_some_handler(const boost::system::error_code& error,
										std::size_t bytes_transfered) 
	{
		ROS_DEBUG("Entered async_read_some_handler method.");
		ScopedLock lock(read_mutex_);
		if (error) //e.g. if no input received from receiver (or ttyACM1 !while! messages sent), bytes_transferred will be 0 of course, error.message() will be "operation canceled"
		{
			ROS_ERROR("mosaic-X5 ASIO input buffer read error: %s, %li",
					error.message().c_str(), bytes_transfered); // c_str() is also part of <string>, str() would not work here
		} 
		else if (bytes_transfered > 0) 
		{
			in_buffer_size_ += bytes_transfered;
	 
			/* uint8_t *pRawDataStart = &(*(in_.begin() + (in_buffer_size_ - bytes_transfered))); //&(*()) seems redundant!
			std::size_t raw_data_stream_size = bytes_transfered;
	 
			if (write_callback_) //bit confusing, write? only from new incoming data. Yes, raw data, nothing to do with doWrite!!
				write_callback_(pRawDataStart, raw_data_stream_size);
	 
			if (debug >= 4) {
				std::ostringstream oss;
				for (std::vector<uint8_t>::iterator it =
					in_.begin() + in_buffer_size_ - bytes_transfered;
				it != in_.begin() + in_buffer_size_; ++it) //Without curly braces, only the first statement following the loop definition is considered to belong to the loop body.
					oss << boost::format("%02x") % static_cast<unsigned int>(*it) << " ";
				ROS_DEBUG("mosaic-X5 received %li bytes \n%s", bytes_transfered,
					oss.str().c_str());
			}*/
	 
			if (read_callback_) //will be false in InitializeSerial (first call)
			{
				ROS_DEBUG("Leaving async_read_some_handler method, with bytes_transferred being %u", (unsigned int) bytes_transfered);
				read_callback_(in_.data(), in_buffer_size_);	 // not just the few bytes above, now all that was read in so far is passed to readCallback from CallbackHandlers class!..
																// .data() Returns a direct pointer to the memory array used internally by the vector to store its owned elements. Because elements in the vector are guaranteed to be stored in contiguous storage locations in the same order as represented by the vector, the pointer retrieved can be offset to access any element in the array.
				//ROS_DEBUG("After read_callback_(in_.data etc");
			}		 
			read_condition_.notify_all(); //other threads can now read too..
		}
	 
		if (!stopping_)
			io_service_->post(boost::bind(&AsyncManager<StreamT>::doRead, this));
	}
	 
	template <typename StreamT>
	void AsyncManager<StreamT>::doClose() 
	{
		ScopedLock lock(read_mutex_);
		stopping_ = true;
		boost::system::error_code error;
		stream_->close(error); 
		if(error)
		{
			ROS_ERROR_STREAM("Error while closing the AsyncManager: " << error.message().c_str());
		}
	}
	 
	template <typename StreamT>
	void AsyncManager<StreamT>::wait(const boost::posix_time::time_duration& timeout) 
	{
		ScopedLock lock(read_mutex_);
		read_condition_.timed_wait(lock, timeout);
		// bool timed_wait(boost::unique_lock<boost::mutex>& lock,boost::system_time const& abs_time)
		// Unlocks mutex and blocks current thread for specified time. The thread will unblock when notified by a call to this->notify_one() (perhaps) or this->notify_all(), when the time as reported by boost::get_system_time() would be equal to or later than the specified abs_time, or spuriously.
		// When the thread is unblocked (for whatever reason), the lock is reacquired by invoking (invoked under the hood, not you) lock.lock() before the call to wait returns (as in std::condition_variable::wait_for).
		// false if the call is returning because the time specified by abs_time was reached (kind of means waiting should be continued), true otherwise. 
	}
}
 
#endif // for ASYNC_MANAGER_HPP
 
 
 