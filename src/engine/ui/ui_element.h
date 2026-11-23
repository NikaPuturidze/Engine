#pragma once

class UIElement
{
	public:
		virtual ~UIElement() = default;
		virtual void Render() const = 0;
};
