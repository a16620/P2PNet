#pragma once
#include <stdexcept>
#include <array>
#include <memory>

template<typename dtype>
class BaseBuffer
{
private:
	int length;
	dtype* buffer;
public:
	void release()
	{
		length = 0;
		if (buffer)
			delete[] buffer;
		buffer = nullptr;
	}

	void reserve(int _size)
	{
		if (_size <= 0)
		{
			release();
			throw std::runtime_error("buffer size can't be under zero");
		}
		length = _size;
		if (buffer)
			delete[] buffer;
		buffer = new dtype[length];
	}

	int size() const
	{
		return length;
	}

	dtype* getBuffer() const
	{
		return buffer;
	}

	BaseBuffer()
	{
		length = 0;
		buffer = nullptr;
	}

	BaseBuffer(int size)
	{
		reserve(size);
	}

	BaseBuffer(BaseBuffer&& o) noexcept
	{
		length = o.length;
		buffer = o.buffer;

		o.length = 0;
		o.buffer = nullptr;

	}

	BaseBuffer& operator=(BaseBuffer&& o) {
		length = o.length;
		buffer = o.buffer;

		o.length = 0;
		o.buffer = nullptr;

		return *this;
	}

	~BaseBuffer()
	{
		if (buffer)
			delete[] buffer;
	}
};

using CBuffer = BaseBuffer<char>;

template <class T, int N>
class SequentArrayList {
	T arr[N];
	int last;
public:
	SequentArrayList() {
		last = -1;
	}

	auto& getArray() const {
		return arr;
	}

	void push(T& e) {
		last++;
		arr[last] = e;
	}

	void push(T&& e) {
		last++;
		arr[last] = std::move(e);
	}

	void pop(int n) {
		arr[n] = arr[last--];
	}

	T& at(int n) const {
		return arr[n];
	}

	int size() const {
		return last+1;
	}
};

template <int N>
class RecyclerBuffer {
	BYTE buffer[N];
	int start;
	int end;
	int used;
public:
	RecyclerBuffer() : start(0), end(0), used(N) {

	}

	auto push(const CBuffer& buf) {
		const int&& sz = buf.size();
		if (used + sz > N)
			return false;

		if (end + sz > N) {
			Arrange();
		}

		memcpy(buffer+end, buf.getBuffer(), sz);
		end += sz;
		used += sz;

		return true;
	}

	auto poll(CBuffer& dst, size_t sz) {
		if (used < sz) {
			return false;
		}

		memcpy(dst.getBuffer(), buffer + start, sz);
		start += sz;
		used -= sz;

		return true;
	}
private:
	void Arrange() {
		if (!start || !used)
			return;
		BYTE* tmpB = malloc(used);
		memcpy(tmpB, buffer + start, used);
		memcpy(buffer, tmpB, used);
		free(tmpB);
		start = 0;
		end = used;
	}
};