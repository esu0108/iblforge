#include "app/EventLoop.h"

void RunEventLoop( App& app )
{
	FrameScheduler& sched = app.Scheduler();
	for( ;; )
	{
		if( sched.ShouldSleep( app.Minimized(), app.ContinuousRender() ) )
		{
			const DWORD timeout = FrameScheduler::SleepTimeoutMs( app.Minimized(), app.Busy(), app.WantsTextInput() );
			MsgWaitForMultipleObjectsEx( 0, nullptr, timeout, QS_ALLINPUT, MWMO_INPUTAVAILABLE );
		}

		MSG msg;
		bool gotMessage = false;
		bool quit = false;
		while( PeekMessageW( &msg, nullptr, 0, 0, PM_REMOVE ) )
		{
			gotMessage = true;
			if( msg.message == WM_QUIT ) quit = true;
			TranslateMessage( &msg );
			DispatchMessageW( &msg );
		}
		if( gotMessage ) sched.NoteMessage();
		if( quit ) return;
		if( app.Minimized() ) continue; // 最小化中は上の待機で眠る (復帰は WM_SIZE で起きる)

		app.RenderFrame();
	}
}
