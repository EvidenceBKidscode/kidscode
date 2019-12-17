# LuaJIT - Kidscode specific

if(MINGW)
	if (CMAKE_COMPILER_PREFIX)
		add_custom_target(LuaJit
			COMMAND make HOST_CC="gcc" CROSS=${CMAKE_COMPILER_PREFIX} TARGET_SYS=Windows luajit.o libluajit.a
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/luajit/src
		)
		set(LUA_LIBRARY luajit)
		set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/luajit/src)
		link_directories(${CMAKE_SOURCE_DIR}/lib/luajit/src)
		set(USE_LUAJIT TRUE)
		set(LUAJIT_FOUND TRUE)
	else()
		message(ERROR "CMAKE_COMPILER_PREFIX not set, needed for LuaJIT compliation.")
	endif()
else()
	add_custom_target(LuaJit
		COMMAND make luajit.o libluajit.a
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/luajit/src
	)
	set(LUA_LIBRARY luajit)
	set(LUA_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/luajit/src)
	link_directories(${CMAKE_SOURCE_DIR}/lib/luajit/src)
	set(USE_LUAJIT TRUE)
	set(LUAJIT_FOUND TRUE)
endif()
