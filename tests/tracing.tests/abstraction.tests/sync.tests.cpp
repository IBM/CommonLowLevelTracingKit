// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#include "abstraction/sync.h"
#include "abstraction/file.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <array>
#include <barrier>
#include <csignal>
#include <errno.h>
#include <iterator>
#include <memory>
#include <ostream>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <tuple>
#include <unistd.h>
using namespace ::testing;
using namespace ::testing::internal;

#if defined(__SANITIZE_ADDRESS__) || \
	(defined(__clang__) && defined(__has_feature) && __has_feature(address_sanitizer))
static constexpr bool AsanEnabled = true;
#else
static constexpr bool AsanEnabled = false;
#endif

using namespace std::string_literals;

TEST(sync, global_mutex_in_thread)
{
	std::array<std::unique_ptr<std::thread>, 1024> threads;
	std::barrier thread_count{threads.size()};

	int counter = 0;
	auto thread_function = [&thread_count, &counter](const int index) -> void {
		thread_count.arrive_and_wait();
		{
			syn_lock_object_t lock SYNC_CLEANUP(syn_global_lock_release) = sync_global_lock_get();
			counter += index;
		}
	};

	// start threads
	for (uint64_t index = 0; index < threads.size(); index++) {
		auto &thread = threads.at(index);
		thread = std::make_unique<std::thread>(thread_function, index);
	}

	// join threads
	for (auto &thread : threads) {
		thread->join();
	}

	// check result
	{
		const int n = std::ssize(threads) - 1;
		const int should_be = n * (n + 1) / 2;
		EXPECT_EQ(should_be, counter);
	}
}

TEST(sync, memory_mutex_in_thread)
{
	std::array<std::unique_ptr<std::thread>, 1024> threads;
	std::barrier thread_count{threads.size()};
	sync_mutex_t mutex = {};
	sync_memory_mutex_init(&mutex);

	int counter = 0;
	auto thread_function = [&thread_count, &counter, &mutex](const int index) -> void {
		thread_count.arrive_and_wait();
		{
			syn_lock_object_t lock SYNC_CLEANUP(sync_memory_mutex_release) =
				sync_memory_mutex_get(&mutex);
			counter += index;
		}
	};

	// start threads
	for (uint64_t index = 0; index < threads.size(); index++) {
		auto &thread = threads.at(index);
		thread = std::make_unique<std::thread>(thread_function, index);
	}

	// join threads
	for (auto &thread : threads) {
		thread->join();
	}

	// check result
	{
		const int n = std::ssize(threads) - 1;
		const int should_be = n * (n + 1) / 2;
		EXPECT_EQ(should_be, counter);
	}
}

TEST(sync, memory_mutex_in_ended_thread)
{
	sync_mutex_t mutex = {};
	sync_memory_mutex_init(&mutex);

	auto child_thread_function = [&mutex]() -> void {
		std::ignore = sync_memory_mutex_get(&mutex);
	};
	std::thread child_thread(child_thread_function);
	child_thread.join();
	CaptureStderr();
	{ // try to get locked mutex with dead owner, should be recovered
		SYNC_MEMORY_LOCK(lock, &mutex);
		EXPECT_TRUE(lock.locked);
	}
	const std::string output = GetCapturedStderr();
	EXPECT_EQ(output, "");
}
TEST(sync, memory_mutex_in_killed_process)
{
	if (AsanEnabled)
		return;
	file_t *file = file_create_temp("memory_mutex_in_killed_process", sizeof(sync_mutex_t));
	sync_mutex_t *const mutex = (sync_mutex_t *)file_mmap_ptr(file);
	sync_memory_mutex_init(mutex);
	{
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		ASSERT_EQ(true, lock.locked);
		sync_memory_mutex_release(&lock);
	}

	pid_t childPid = fork();
	if (childPid < 0) {
		FAIL() << "Failed to fork child process" << std::endl;
	} else if (childPid == 0) {
		// Child process
		sync_memory_mutex_init(mutex);
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		ASSERT_EQ(true, lock.locked);
		kill(getpid(), SIGTERM);
	} else {
		// Parent process

		int status = 0;
		waitpid(childPid, &status, 0); // wait for child to exit
		ASSERT_TRUE(WTERMSIG(status))
			<< "child process not killed by sigkill but by " << WTERMSIG(status);

		CaptureStderr();
		{ // try to get locked mutex with dead owner, should be recovered
			SYNC_MEMORY_LOCK(lock, mutex);
			EXPECT_TRUE(lock.locked) << " could not get mutex after owner died";
			EXPECT_EQ(lock.error_msg, "mutex recovered from dead owner"s);
		}
		const std::string output = GetCapturedStderr();
		EXPECT_EQ(output, "");
		kill(childPid, SIGKILL);
		file_drop(&file);
		return;
	}
}

TEST(sync, memory_mutex_in_exit_process)
{
	if (AsanEnabled)
		return;
	file_t *file = file_create_temp("memory_mutex_in_exit_process", sizeof(sync_mutex_t));
	sync_mutex_t *const mutex = (sync_mutex_t *)file_mmap_ptr(file);
	sync_memory_mutex_init(mutex);
	{
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		ASSERT_EQ(true, lock.locked);
		sync_memory_mutex_release(&lock);
	}

	pid_t childPid = fork();
	if (childPid < 0) {
		FAIL() << "Failed to fork child process" << std::endl;
	} else if (childPid == 0) {
		// Child process
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		ASSERT_EQ(true, lock.locked);
		exit(0);

	} else {
		// Parent process

		int status = 0;
		waitpid(childPid, &status, 0); // wait for child to exit
		ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
			<< "Child process did not exit normally or exited with status " << WEXITSTATUS(status);

		CaptureStderr();
		{ // try to get locked mutex with dead owner, should be recovered
			SYNC_MEMORY_LOCK(lock, mutex);
			EXPECT_TRUE(lock.locked) << " could not get mutex after owner died";
			EXPECT_EQ(lock.error_msg, "mutex recovered from dead owner"s);
		}
		const std::string output = GetCapturedStderr();
		EXPECT_EQ(output, "");
		kill(childPid, SIGKILL);
		return;
	}
}
TEST(sync, memory_mutex_in_exit_process_during_recovery)
{
	if (AsanEnabled)
		return;
	file_t *file =
		file_create_temp("memory_mutex_in_exit_process_during_recovery", sizeof(sync_mutex_t));
	sync_mutex_t *const mutex = (sync_mutex_t *)file_mmap_ptr(file);
	sync_memory_mutex_init(mutex);
	{
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		ASSERT_EQ(true, lock.locked);
		sync_memory_mutex_release(&lock);
	}

	// create first child which will exit holding the lock
	pid_t firstChildPid = fork();
	if (firstChildPid < 0) {
		FAIL() << "Failed to fork first child process" << std::endl;
	} else if (firstChildPid == 0) {
		// Child process
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		if (lock.locked) // as expected
		{
			exit(0);
		}
		exit(1);
	}

	// wait for end of first child
	{
		int status = 0;
		waitpid(firstChildPid, &status, 0); // wait for child to exit
		ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
			<< "first child process did not exit normally or exited with status "
			<< WEXITSTATUS(status);
	}

	// create first child which will exit during lock recovery
	pid_t secondChildPid = fork();
	if (secondChildPid < 0) {
		FAIL() << "Failed to fork second child process" << std::endl;
	} else if (secondChildPid == 0) {
		// Child process
		pthread_mutex_t *const pthread_mutex = (pthread_mutex_t *)mutex;
		const int status = pthread_mutex_lock(pthread_mutex);
		if (status == EOWNERDEAD) { // as expected
			exit(0);
		}
		exit(1);
	}

	// wait for end of second child
	{
		int status = 0;
		waitpid(secondChildPid, &status, 0); // wait for second child to exit
		ASSERT_TRUE(WIFEXITED(status) && WEXITSTATUS(status) == 0)
			<< "second child process did not exit normally or exited with status "
			<< WEXITSTATUS(status);
	}

	CaptureStderr();
	{ // try to get locked mutex with dead owner, should be recovered
		SYNC_MEMORY_LOCK(lock, mutex);
		EXPECT_TRUE(lock.locked) << " could not get mutex after owner died";
		EXPECT_EQ(lock.error_msg, "mutex recovered from dead owner"s);
	}
	const std::string output = GetCapturedStderr();
	EXPECT_EQ(output, "");
}

static sync_mutex_t *static_mutex;
static bool signal_in_same_process__signal_handler_happened;
static void signal_in_same_process__signal_handler(int sig)
{
	ASSERT_EQ(sig, SIGUSR1);
	signal_in_same_process__signal_handler_happened = true;

	syn_lock_object_t lock = sync_memory_mutex_get(static_mutex);
	EXPECT_FALSE(lock.locked);
}
TEST(sync, signal_in_same_process_same_thread)
{
	ASSERT_NE(SIG_ERR, std::signal(SIGUSR1, signal_in_same_process__signal_handler));
	sync_mutex_t mutex;
	sync_memory_mutex_init(&mutex);
	ASSERT_EQ(static_mutex, nullptr);
	static_mutex = &mutex;
	signal_in_same_process__signal_handler_happened = false;
	syn_lock_object_t lock = sync_memory_mutex_get(&mutex);
	EXPECT_TRUE(lock.locked);

	// know that mutex is locked, call signal handler
	ASSERT_EQ(0, kill(getpid(), SIGUSR1));
	ASSERT_TRUE(signal_in_same_process__signal_handler_happened);

	// release lock
	sync_memory_mutex_release(&lock);
	EXPECT_FALSE(lock.locked);

	// try again to lock
	lock = sync_memory_mutex_get(&mutex);
	EXPECT_TRUE(lock.locked);
	sync_memory_mutex_release(&lock);
	EXPECT_FALSE(lock.locked);

	// reset signal handler
	ASSERT_NE(SIG_ERR, std::signal(SIGUSR1, SIG_DFL));
	static_mutex = nullptr;
}

TEST(sync, get_mutex_timeout_with_process)
{
	if (AsanEnabled)
		return;
	file_t *file = file_create_temp("get_mutex_timeout_with_process", sizeof(sync_mutex_t) + 1);
	sync_mutex_t *const mutex = (sync_mutex_t *)file_mmap_ptr(file);
	sync_memory_mutex_init(mutex);
	{
		syn_lock_object_t lock = sync_memory_mutex_get(mutex);
		ASSERT_EQ(true, lock.locked);
		sync_memory_mutex_release(&lock);
	}
	bool &child_done = *std::bit_cast<bool *>(mutex + 1);

	// Create child process
	pid_t childPid = fork();
	if (childPid < 0) {
		FAIL() << "Failed to fork child process" << std::endl;
	} else if (childPid == 0) {
		// Child process
		syn_lock_object_t locked = sync_memory_mutex_get(mutex);
		EXPECT_TRUE(locked.locked);
		child_done = 1;
		while (1) { // wait to be killed by parent, do nothing
		}
	} else {
		// Parent process
		while (child_done == 0) { // wait till child has locked mutex
		}
		CaptureStderr();
		syn_lock_object_t locked = sync_memory_mutex_get(mutex);
		const std::string output = GetCapturedStderr();
		EXPECT_FALSE(locked.locked);
		EXPECT_EQ(output, "");
		EXPECT_EQ(locked.error_msg, "Connection timed out"s);
		kill(childPid, SIGKILL);
		return;
	}
}

TEST(sync, release_global_twice)
{
	auto lock = sync_global_lock_get();
	EXPECT_TRUE(lock.locked);
	syn_global_lock_release(&lock);
	EXPECT_EXIT(syn_global_lock_release(&lock), ::testing::ExitedWithCode(1),
				"clltk unrecoverable");
}

TEST(sync, release_memory_twice)
{
	sync_mutex_t mutex;
	sync_memory_mutex_init(&mutex);
	auto lock = sync_memory_mutex_get(&mutex);
	EXPECT_TRUE(lock.locked);
	sync_memory_mutex_release(&lock);
	CaptureStderr();
	sync_memory_mutex_release(&lock);
	const std::string output = GetCapturedStderr();
	EXPECT_THAT(output, ::testing::HasSubstr("clltk recoverable"));
}

TEST(sync, release_invalid_memory_mutex)
{
	{
		CaptureStderr();
		CaptureStdout();
		syn_lock_object_t lock{};
		lock.error_msg = "dummy",
		sync_memory_mutex_release(&lock); // not locked object
		EXPECT_EQ(GetCapturedStdout(), "") << "should not print message";
		EXPECT_EQ(GetCapturedStderr(), "") << "should not print error";
	}
	{
		CaptureStdout();
		CaptureStderr();
		syn_lock_object_t lock{};
		lock.error_msg = "\0";
		sync_memory_mutex_release(&lock); // not locked object
		EXPECT_EQ(GetCapturedStdout(), "") << "should not print message";
		EXPECT_THAT(
			GetCapturedStderr(),
			MatchesRegex(R"(.*clltk recoverable: releasing an unlocked mutex is not allowed.*)"))
			<< "should not print error";
	}
	{
		syn_lock_object_t lock{};
		lock.locked = 1;
		EXPECT_EXIT(
			{ sync_memory_mutex_release(&lock); }, ExitedWithCode(1),
			".*clltk unrecoverable: releasing a NULL lock is not allowed.*");
	}
}

TEST(sync, sync_memory_mutex_get_twice)
{
	sync_mutex_t mutex;
	sync_memory_mutex_init(&mutex);
	auto lock0 = sync_memory_mutex_get(&mutex);
	EXPECT_TRUE(lock0.locked);
	// test second lock
	CaptureStderr();
	auto lock1 = sync_memory_mutex_get(&mutex);
	EXPECT_FALSE(lock1.locked);
	// check stderr
	const std::string output = GetCapturedStderr();
	EXPECT_THAT(output, ::testing::IsEmpty());
	// check error message
	EXPECT_THAT(std::string(lock1.error_msg), "Resource deadlock avoided");
}