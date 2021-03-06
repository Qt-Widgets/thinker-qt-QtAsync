//
// thinker.h
// This file is part of Thinker-Qt
// Copyright (C) 2010-2014 HostileFork.com
//
// Thinker-Qt is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Thinker-Qt is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Thinker-Qt.  If not, see <http://www.gnu.org/licenses/>.
//
// See http://hostilefork.com/thinker-qt/ for more information on this project
//

#ifndef THINKERQT_THINKERBASE_H
#define THINKERQT_THINKERBASE_H

#include <QObject>
#include <QSet>
#include <QReadWriteLock>

#include "defs.h"
#include "snapshottable.h"
#include "signalthrottler.h"
#include "thinkerpresent.h"
#include "thinkerpresentwatcher.h"

class ThinkerManager;
class ThinkerRunner;
class ThinkerPresentWatcherBase;

//
// ThinkerBase
//
// A "Thinker" is a task which runs on its own thread and is supposed to make
// some kind of calculation which other threads are interested in.  The way
// that progress is communicated back is through read-only "snapshots" of
// the object's state.
//
// The reason there is a base "ThinkerBase" which is separate from the
// "Thinker" template is due to limitations of Qt's moc in allowing you to
// declare templated QObjects.  See this article for more information:
//
//  http://doc.trolltech.com/qq/qq15-academic.html
//

class ThinkerBase : public QObject, virtual public SnapshottableBase
{
    Q_OBJECT

    friend class ThinkerManager;
    friend class ThinkerPresentWatcherBase;
    friend class ThinkerPresentBase;
    friend class ThinkerRunner;

  private:
    enum class State {
        ThinkerOwnedByRunner = 0,
        ThinkerFinished = 1,
        ThinkerCanceled = 2
    };

  public:
  #ifdef THINKERQT_EXPLICIT_MANAGER
    ThinkerBase (ThinkerManager & mgr);
  #else
    ThinkerBase ();
  #endif

    virtual ~ThinkerBase () override;

  public:
    ThinkerManager & getManager() const;

  public:
    bool wasPauseRequested(unsigned long time = 0) const;

  #ifndef Q_NO_EXCEPTIONS
    //
    // If exceptions are enabled, you can use this and it will throw an
    // exception to the internal thread loop boilerplate on a pause request;
    // only appropriate for non-continuable thinkers to use...
    //
    void pollForStopException(unsigned long time = 0) const;
  #endif

  public:
    virtual void afterThreadAttach();
    virtual void beforeThreadDetach();

  public:
    bool hopefullyCurrentThreadIsThink(codeplace const & cp) const {
        //
        // we currently allow locking a thinker for writing on the manager
        // thread between the time the Snapshot base class constructor has run
        // and when it is attached to a ThinkerPresent
        //
        return hopefully(thread() == QThread::currentThread(), cp);
    }

    // These overrides provide added checking and also signal "progress" when
    // the unlock runs.
    //
  protected:
    virtual void lockForWrite(codeplace const & cp) override;
    virtual void unlock(codeplace const & cp) override;

  #ifndef THINKERQT_REQUIRE_CODEPLACE
    //
    // This will cause the any asserts to indicate a failure in thinker.h
    // instead of at the offending line in the caller... not as good... see
    // hoist documentation http://hostilefork.com/hoist/
    //
    void lockForWrite()
      { return lockForWrite(HERE); }

    void unlock()
      { return unlock(HERE); }
  #endif

    // The done signal is used by the ThinkerPresentWatcher.  Once it was
    // the responsibility of a thinker's start/resume methods to emit this
    // signal, but that was switched to returning true or false.  Given
    // that change there may be better ways of notifying the watcher of
    // completion...but to keep things working as they were the signal
    // has been left here and is emitted by wrapping functions.
    //
  signals:
    void done();

  private:
    bool startMaybeEmitDone() {
        if (start()) {
            emit done();
            return true;
        }
        return false;
    }

    bool resumeMaybeEmitDone() {
        if (resume()) {
            emit done();
            return true;
        }
        return false;
    }

  protected:
    virtual bool start() = 0;
    virtual bool resume() {
        //
        // Making a restartable thinker typically involves extra work to
        // make it into a coroutine.  You don't have to do that work if
        // you don't intend on pausing and restarting thinkers.  In that
        // case, wasPauseRequested really just means wasStopRequested...
        //
        hopefullyNotReached("Thinker not designed to be resumable.", HERE);
        return false;
    }

  private:
    State _state;
    ThinkerManager & _mgr;
    QReadWriteLock _watchersLock;
    QSet<ThinkerPresentWatcherBase *> _watchers;
};

#endif // THINKERBASE_H
