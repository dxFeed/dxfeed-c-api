// Definition of the class implementing the main library interface

#pragma once

struct IDXFeed;

/* -------------------------------------------------------------------------- */
/*
 *	DefDXFeedFactory class

 *  creates the default implementation of IDXFeed interface
 */
/* -------------------------------------------------------------------------- */

struct DefDXFeedFactory {
	static IDXFeed* CreateInstance ();
};

/* -------------------------------------------------------------------------- */
/*
 *	DXFeedStorage class

 *  stores a single instance of IDXFeed
 */
/* -------------------------------------------------------------------------- */

struct DXFeedStorage {
	static IDXFeed* GetContent () {
	return m_feed;
	}

	static void SetContent (IDXFeed* value) {
	m_feed = value;
	}

private:

	static IDXFeed* m_feed;
};