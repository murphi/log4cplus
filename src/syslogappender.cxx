// Module:  Log4CPLUS
// File:    syslogappender.cxx
// Created: 6/2001
// Author:  Tad E. Smith
//
// Copyright 2001-2017 Tad E. Smith
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <log4cplus/syslogappender.h>
#include <log4cplus/streams.h>
#include <log4cplus/helpers/loglog.h>
#include <log4cplus/helpers/property.h>
#include <log4cplus/helpers/stringhelper.h>
#include <log4cplus/spi/loggingevent.h>
#include <log4cplus/internal/internal.h>
#include <log4cplus/internal/env.h>
#include <log4cplus/thread/syncprims-pub-impl.h>
#include <cstring>

#if defined (LOG4CPLUS_HAVE_SYSLOG_H)
#include <syslog.h>

#else // LOG4CPLUS_HAVE_SYSLOG_H

// The following bits were derived from SUSv4 documentation and
// RFC5424 document.

// priority codes
#define LOG_EMERG   0
#define LOG_ALERT   1
#define LOG_CRIT    2
#define LOG_ERR     3
#define LOG_WARNING 4
#define LOG_NOTICE  5
#define LOG_INFO    6
#define LOG_DEBUG   7

// facility codes
#define LOG_KERN      (0 << 3)
#define LOG_USER      (1 << 3)
#define LOG_MAIL      (2 << 3)
#define LOG_DAEMON    (3 << 3)
#define LOG_AUTH      (4 << 3)
#define LOG_SYSLOG    (5 << 3)
#define LOG_LPR       (6 << 3)
#define LOG_NEWS      (7 << 3)
#define LOG_UUCP      (8 << 3)
#define LOG_CRON      (9 << 3)
#define LOG_AUTHPRIV (10 << 3)
#define LOG_FTP      (11 << 3)
#define LOG_NTP      (12 << 3)
#define LOG_SECURITY (13 << 3)
#define LOG_CONSOLE  (14 << 3)
// (15 << 3) is missing here
#define LOG_LOCAL0   (16 << 3)
#define LOG_LOCAL1   (17 << 3)
#define LOG_LOCAL2   (18 << 3)
#define LOG_LOCAL3   (19 << 3)
#define LOG_LOCAL4   (20 << 3)
#define LOG_LOCAL5   (21 << 3)
#define LOG_LOCAL6   (22 << 3)
#define LOG_LOCAL7   (23 << 3)

#endif // LOG4CPLUS_HAVE_SYSLOG_H


namespace log4cplus
{

namespace
{

static
const char*
useIdent (const std::string& string)
{
    if (string.empty ())
        return nullptr;
    else
        return string.c_str ();
}


#ifdef LOG_USER
int const fallback_facility = LOG_USER;

#else
int const fallback_facility = 0;

#endif


static
int
parseFacility (const tstring& text)
{
    if (text.empty ())
        return fallback_facility;
#ifdef LOG_AUTH
    else if (text == LOG4CPLUS_TEXT ("auth"))
        return LOG_AUTH;
#endif
#ifdef LOG_AUTHPRIV
    else if (text == LOG4CPLUS_TEXT ("authpriv"))
        return LOG_AUTHPRIV;
#endif
#ifdef LOG_CONSOLE
    else if (text == LOG4CPLUS_TEXT ("console"))
        return LOG_CONSOLE;
#endif
#ifdef LOG_CRON
    else if (text == LOG4CPLUS_TEXT ("cron"))
        return LOG_CRON;
#endif
#ifdef LOG_DAEMON
    else if (text == LOG4CPLUS_TEXT ("daemon"))
        return LOG_DAEMON;
#endif
#ifdef LOG_FTP
    else if (text == LOG4CPLUS_TEXT ("ftp"))
        return LOG_FTP;
#endif
#ifdef LOG_KERN
    else if (text == LOG4CPLUS_TEXT ("kern"))
        return LOG_KERN;
#endif
#ifdef LOG_LOCAL0
    else if (text == LOG4CPLUS_TEXT ("local0"))
        return LOG_LOCAL0;
#endif
#ifdef LOG_LOCAL1
    else if (text == LOG4CPLUS_TEXT ("local1"))
        return LOG_LOCAL1;
#endif
#ifdef LOG_LOCAL2
    else if (text == LOG4CPLUS_TEXT ("local2"))
        return LOG_LOCAL2;
#endif
#ifdef LOG_LOCAL3
    else if (text == LOG4CPLUS_TEXT ("local3"))
        return LOG_LOCAL3;
#endif
#ifdef LOG_LOCAL4
    else if (text == LOG4CPLUS_TEXT ("local4"))
        return LOG_LOCAL4;
#endif
#ifdef LOG_LOCAL5
    else if (text == LOG4CPLUS_TEXT ("local5"))
        return LOG_LOCAL5;
#endif
#ifdef LOG_LOCAL6
    else if (text == LOG4CPLUS_TEXT ("local6"))
        return LOG_LOCAL6;
#endif
#ifdef LOG_LOCAL7
    else if (text == LOG4CPLUS_TEXT ("local7"))
        return LOG_LOCAL7;
#endif
#ifdef LOG_LPR
    else if (text == LOG4CPLUS_TEXT ("lpr"))
        return LOG_LPR;
#endif
#ifdef LOG_MAIL
    else if (text == LOG4CPLUS_TEXT ("mail"))
        return LOG_MAIL;
#endif
#ifdef LOG_NEWS
    else if (text == LOG4CPLUS_TEXT ("news"))
        return LOG_NEWS;
#endif
#ifdef LOG_NTP
    else if (text == LOG4CPLUS_TEXT ("ntp"))
        return LOG_NTP;
#endif
#ifdef LOG_SECURITY
    else if (text == LOG4CPLUS_TEXT ("security"))
        return LOG_SECURITY;
#endif
#ifdef LOG_SYSLOG
    else if (text == LOG4CPLUS_TEXT ("syslog"))
        return LOG_SYSLOG;
#endif
#ifdef LOG_USER
    else if (text == LOG4CPLUS_TEXT ("user"))
        return LOG_USER;
#endif
#ifdef LOG_UUCP
    else if (text == LOG4CPLUS_TEXT ("uucp"))
        return LOG_UUCP;
#endif
    else
    {
        // Unknown facility.
        tstring msg (LOG4CPLUS_TEXT ("Unknown syslog facility: "));
        msg += text;
        helpers::getLogLog ().error (msg);

        return fallback_facility;
    }
}


static
tstring
substrOrNil(tstring const & str, tstring::size_type const limit)
{
    if (str.empty ())
        return tstring (LOG4CPLUS_TEXT ("-"));
    else
        return str.substr (0, limit);
}

} // namespace


///////////////////////////////////////////////////////////////////////////////
// SysLogAppender ctors and dtor
///////////////////////////////////////////////////////////////////////////////

#if defined (LOG4CPLUS_HAVE_SYSLOG_H)
SysLogAppender::SysLogAppender(const tstring& id)
    : ident(id)
    , facility (0)
    , appendFunc (&SysLogAppender::appendLocal)
    , port (0)
    , remoteSyslogType ()
    , connected (false)
    // Store std::string form of ident as member of SysLogAppender so
    // the address of the c_str() result remains stable for openlog &
    // co to use even if we use wstrings.
    , identStr(LOG4CPLUS_TSTRING_TO_STRING (id) )
    , hostname (helpers::getHostname (true).value_or (LOG4CPLUS_C_STR_TO_TSTRING ("-")))
{
    ::openlog(useIdent(identStr), 0, 0);
}

#endif


SysLogAppender::SysLogAppender(const helpers::Properties & properties)
    : Appender(properties)
    , facility (0)
    , appendFunc (nullptr)
    , port (0)
    , connected (false)
{
    ident = properties.getProperty( LOG4CPLUS_TEXT("ident") );
    facility = parseFacility (
        helpers::toLower (
            properties.getProperty (LOG4CPLUS_TEXT ("facility"))));
    identStr = LOG4CPLUS_TSTRING_TO_STRING (ident);

    bool udp = true;
    properties.getBool (udp, LOG4CPLUS_TEXT ("udp"));
    remoteSyslogType = udp ? RSTUdp : RSTTcp;

    properties.getBool (ipv6, LOG4CPLUS_TEXT ("IPv6"));

    bool fqdn = true;
    properties.getBool (fqdn, LOG4CPLUS_TEXT ("fqdn"));
    hostname = std::move(helpers::getHostname (fqdn).value_or (LOG4CPLUS_C_STR_TO_TSTRING ("-")));

    properties.getString (host, LOG4CPLUS_TEXT ("host"))
      || properties.getString (host, LOG4CPLUS_TEXT ("SyslogHost"));
    if (host.empty ())
    {
#if defined (LOG4CPLUS_HAVE_SYSLOG_H)
        appendFunc = &SysLogAppender::appendLocal;
        ::openlog(useIdent(identStr), 0, 0);

#else
        helpers::getLogLog ().error (
            LOG4CPLUS_TEXT ("SysLogAppender")
            LOG4CPLUS_TEXT ("- local syslog not available"), true);

#endif
    }
    else
    {
        if (! properties.getInt (port, LOG4CPLUS_TEXT ("port")))
            port = 514;

        appendFunc = &SysLogAppender::appendRemote;
        openSocket ();
        initConnector ();
    }
}


SysLogAppender::SysLogAppender(const tstring& id, const tstring & h,
    int p, const tstring & f, SysLogAppender::RemoteSyslogType rst,
    bool ipv6_ /*= false*/, bool fqdn /*= true*/)
    : ident (id)
    , facility (parseFacility (helpers::toLower (f)))
    , appendFunc (&SysLogAppender::appendRemote)
    , host (h)
    , port (p)
    , remoteSyslogType (rst)
    , connected (false)
    , ipv6 (ipv6_)
    // Store std::string form of ident as member of SysLogAppender so
    // the address of the c_str() result remains stable for openlog &
    // co to use even if we use wstrings.
    , identStr(LOG4CPLUS_TSTRING_TO_STRING (id) )
    , hostname (helpers::getHostname (fqdn).value_or (LOG4CPLUS_C_STR_TO_TSTRING ("-")))
{
    openSocket ();
    initConnector ();
}


SysLogAppender::~SysLogAppender()
{
    destructorImpl();
}



///////////////////////////////////////////////////////////////////////////////
// SysLogAppender public methods
///////////////////////////////////////////////////////////////////////////////

void
SysLogAppender::close()
{
    helpers::getLogLog().debug(
        LOG4CPLUS_TEXT("Entering SysLogAppender::close()..."));
    thread::MutexGuard guard (access_mutex);

    if (host.empty ())
    {
#if defined (LOG4CPLUS_HAVE_SYSLOG_H)
        ::closelog();
#endif
    }
    else
        syslogSocket.close ();

#if ! defined (LOG4CPLUS_SINGLE_THREADED)
    if (connector)
        connector->terminate ();
#endif

    closed = true;
}



///////////////////////////////////////////////////////////////////////////////
// SysLogAppender protected methods
///////////////////////////////////////////////////////////////////////////////

int
SysLogAppender::getSysLogLevel(const LogLevel& ll) const
{
    if(ll < INFO_LOG_LEVEL /* || ll < DEBUG_LOG_LEVEL*/) {
        return LOG_DEBUG;
    }
    else if(ll < WARN_LOG_LEVEL) {
        return LOG_INFO;
    }
    else if(ll < ERROR_LOG_LEVEL) {
        return LOG_WARNING;
    }
    else if(ll < FATAL_LOG_LEVEL) {
        return LOG_ERR;
    }
    else if(ll == FATAL_LOG_LEVEL) {
        return LOG_CRIT;
    }

    return LOG_ALERT;  // ll > FATAL_LOG_LEVEL
}


// This method does not need to be locked since it is called by
// doAppend() which performs the locking
void
SysLogAppender::append(const spi::InternalLoggingEvent& event)
{
    (this->*appendFunc) (event);
}


#if defined (LOG4CPLUS_HAVE_SYSLOG_H)
void
SysLogAppender::appendLocal(const spi::InternalLoggingEvent& event)
{
    int const level = getSysLogLevel(event.getLogLevel());
    internal::appender_sratch_pad & appender_sp = internal::get_appender_sp ();
    detail::clear_tostringstream (appender_sp.oss);
    layout->formatAndAppend(appender_sp.oss, event);
    appender_sp.str = appender_sp.oss.str ();
    ::syslog(facility | level, "%s",
        LOG4CPLUS_TSTRING_TO_STRING(appender_sp.str).c_str());
}

#endif


tstring const SysLogAppender::remoteTimeFormat (
    LOG4CPLUS_TEXT ("%Y-%m-%dT%H:%M:%S.%qZ"));


void
SysLogAppender::appendRemote(const spi::InternalLoggingEvent& event)
{

    if (! connected)
    {
#if ! defined (LOG4CPLUS_SINGLE_THREADED)
        connector->trigger ();
        return;

#else
        openSocket ();
        if (! connected)
        {
            helpers::getLogLog ().error (
                LOG4CPLUS_TEXT ("SysLogAppender")
                LOG4CPLUS_TEXT ("- failed to connect to ")
                + host + LOG4CPLUS_TEXT (":")
                + helpers::convertIntegerToString (port));
            return;
        }
#endif
    }

    int const level = getSysLogLevel(event.getLogLevel());
    internal::appender_sratch_pad & appender_sp = internal::get_appender_sp ();
    detail::clear_tostringstream (appender_sp.oss);

    appender_sp.oss
        // PRI
        << LOG4CPLUS_TEXT ('<') << (level | facility) << LOG4CPLUS_TEXT ('>')
        // VERSION
        << 1
        // TIMESTAMP
        << LOG4CPLUS_TEXT (' ')
        << helpers::getFormattedTime (remoteTimeFormat, event.getTimestamp (),
            true)
        // HOSTNAME
        << LOG4CPLUS_TEXT (' ') << substrOrNil (hostname, 255)
        // APP-NAME
        << LOG4CPLUS_TEXT (' ') << substrOrNil (ident, 48)
        // PROCID
        << LOG4CPLUS_TEXT (' ') << internal::get_process_id ()
        // MSGID
        << LOG4CPLUS_TEXT (' ') << substrOrNil (event.getLoggerName (), 32)
        // STRUCTURED-DATA
        // no structured data, it could be whole MDC
        << LOG4CPLUS_TEXT (" - ");

    // MSG
    layout->formatAndAppend (appender_sp.oss, event);

    appender_sp.chstr = LOG4CPLUS_TSTRING_TO_STRING (appender_sp.oss.str ());

    if (remoteSyslogType != RSTUdp)
    {
        // see (RFC6587, 3.4.1 Octet
        // Counting)[http://tools.ietf.org/html/rfc6587#section-3.4.1]
        std::string syslogFrameHeader (
            helpers::convertIntegerToNarrowString (appender_sp.chstr.size ()));
        syslogFrameHeader += ' ';
        appender_sp.chstr.insert (appender_sp.chstr.begin (),
            syslogFrameHeader.begin (), syslogFrameHeader.end ());
    }

    bool ret = syslogSocket.write (appender_sp.chstr);
    if (! ret)
    {
        helpers::getLogLog ().warn (
            LOG4CPLUS_TEXT ("SysLogAppender::appendRemote")
            LOG4CPLUS_TEXT ("- socket write failed"));

        connected = false;

#if ! defined (LOG4CPLUS_SINGLE_THREADED)
        connector->trigger ();
#endif
    }
}


#if ! defined (LOG4CPLUS_SINGLE_THREADED)
thread::Mutex const &
SysLogAppender::ctcGetAccessMutex () const
{
    return access_mutex;
}


helpers::Socket &
SysLogAppender::ctcGetSocket ()
{
    return syslogSocket;
}


helpers::Socket
SysLogAppender::ctcConnect ()
{
    return helpers::Socket (host, static_cast<unsigned short>(port),
        remoteSyslogType == RSTUdp);
}


void
SysLogAppender::ctcSetConnected ()
{
    connected = true;
}

#endif


void
SysLogAppender::initConnector ()
{
#if ! defined (LOG4CPLUS_SINGLE_THREADED)
    connected = true;
    connector = new helpers::ConnectorThread (*this);
    connector->start ();
#endif
}


void
SysLogAppender::openSocket ()
{
    syslogSocket = helpers::Socket (host, static_cast<unsigned short>(port),
        remoteSyslogType == RSTUdp, ipv6);
    connected = syslogSocket.isOpen ();
    if (! connected)
        helpers::getLogLog ().error (
            LOG4CPLUS_TEXT ("SysLogAppender")
            LOG4CPLUS_TEXT ("- failed to connect to ")
            + host
            + LOG4CPLUS_TEXT (":") + helpers::convertIntegerToString (port));
}


} // namespace log4cplus
