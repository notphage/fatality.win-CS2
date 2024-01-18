#pragma once

#include <MinHook.h>
#include <utils/value_obfuscation.h>

namespace utils
{
class hook_interface
{
public:
	hook_interface() = default;
	hook_interface(hook_interface const &other) = delete;
	hook_interface &operator=(hook_interface const &other) = delete;
	hook_interface(hook_interface &&other) = delete;
	hook_interface &operator=(hook_interface &&other) = delete;

	virtual ~hook_interface() = default;
	virtual void attach() = 0;
	virtual void detach() = 0;
	virtual uintptr_t get_endpoint() const = 0;
	virtual uintptr_t get_target() const = 0;
};

template <typename T> class hook final : public hook_interface
{
public:
	explicit hook(const encrypted_ptr<T> target, const encrypted_ptr<T> endpoint)
		: hook_interface(), target(target), is_attached(false)
	{
		void *org;
		MH_CreateHook(target(), endpoint(), &org);
		trampoline = encrypted_ptr<T>(uintptr_t(org));
	}

	~hook()
	{
		if (is_attached)
		{
			try
			{
				hook<T>::detach();
			}
			catch (const std::runtime_error &)
			{
				// Fine then.
			}
		}

		MH_RemoveHook(target());
	}

	void attach() override
	{
		MH_EnableHook(target());
		is_attached = true;
	}

	void detach() override
	{
		MH_DisableHook(target());
		is_attached = false;
	}

	uintptr_t get_endpoint() const override { return reinterpret_cast<uintptr_t>(endpoint()); }

	uintptr_t get_target() const override { return reinterpret_cast<uintptr_t>(target()); }

	T *get_original() const { return trampoline(); }

	template <typename... A> auto call(const A... args) const { return get_original()(args...); }

private:
	void set_endpoint(const encrypted_ptr<T> endpoint)
	{
		const auto was_attached = is_attached;

		if (is_attached)
			detach();

		this->endpoint = endpoint;

		if (this->endpoint && was_attached)
			attach();
	}

	encrypted_ptr<T> target, endpoint, trampoline;
	bool is_attached;
};
}
