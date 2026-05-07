/*
 * Copyright 2025, Andrea Anzani <andrea.anzani@gmail.com>.
 * All rights reserved. Distributed under the terms of the MIT License.
 */
#ifndef STYLE_COLOURS_DATA_H
#define STYLE_COLOURS_DATA_H

#include <InterfaceDefs.h>


// StyleColoursData encapsulates foreground and background color settings.
// It supports both color_which (system colors) and rgb_color (custom colors).
// This class can be used as a base class or composed.
class StyleColoursData {
public:
	StyleColoursData();
	StyleColoursData(const StyleColoursData& other);

	bool operator==(const StyleColoursData& other) const;
	bool operator!=(const StyleColoursData& other) const;


	// Getters for foreground color
	inline rgb_color	ForegroundColor() const { return fFgColor; }
	inline bool			HasForegroundColor() const { return fHasFgColor; }

	// Getters for background color
	inline rgb_color 	BackgroundColor() const { return fBgColor; }
	inline bool 		HasBackgroundColor() const { return fHashBgColor; }

protected:
	StyleColoursData& operator=(const StyleColoursData& other);
		// Setters for background color
	void SetBackgroundColor(color_which which);
	void SetBackgroundColor(rgb_color color);

	// Setters for foreground color
	void SetForegroundColor(color_which which);
	void SetForegroundColor(rgb_color color);

private:
	bool fHasFgColor;
	bool fHashBgColor;

	rgb_color fFgColor;
	rgb_color fBgColor;
};


#endif // STYLE_COLOURS_DATA_H
