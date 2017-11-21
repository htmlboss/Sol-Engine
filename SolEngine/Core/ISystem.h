#pragma once

class ISystem {

public:
	explicit constexpr ISystem() = default;
	virtual ~ISystem() = default;

	virtual void init() = 0;
	virtual void update(const float delta) = 0;
	virtual void shutdown() = 0;
};