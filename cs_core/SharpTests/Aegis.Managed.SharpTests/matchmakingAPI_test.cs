using NUnit.Framework;
using Aegis.Managed.Matchmaking;
using MatchType = Aegis.Managed.Matchmaking.MatchType;
using System;

namespace Aegis.Managed.SharpTests
{
    [TestFixture]
    public class MatchmakingAPITests
    {
        //! Initial State
        
        [Test]
        public void NewInstance_GenreDefaults_ToNone()
        {
            var api = new NoSkillBaseAPI();
            Assert.That(api.ActiveGenre, Is.EqualTo(GenreID.None));
        }

        [Test]
        public void NewInstance_LastReqMatchType_DefaultsToNormal()
        {
            var api = new NoSkillBaseAPI();
            Assert.That(api.LastReqMatchType, Is.EqualTo(MatchType.Normal));
        }

        //! Request Normal Match

        [Test]
        public void RequestNormalMatch_InvokeCallback()
        {
            var api = new NoSkillBaseAPI();
            bool called = false;

            api.RequestNormalMatch(1, (ip, port) => called = true);
            Assert.That(called, Is.True);
        }

        [Test]
        public void RequestNormalMatch_CallbackRcvs_NonEmptyHostInfo()
        {
            var api = new NoSkillBaseAPI();
            string? receivedIP = null;
            ushort receivedPort = 0;

            api.RequestNormalMatch(1, (ip, port) =>
            {
                receivedIP = ip;
                receivedPort = port;
            });

            Assert.That(receivedIP, Is.EqualTo("127.0.0.1"));
            Assert.That(receivedPort, Is.EqualTo((ushort)7777));
        }

        //! Request Ranked/Unranked/Normal match

        [Test]
        public void RequestRanked_SetsLastReqMatch_ToRanked()
        {
            var api = new NoSkillBaseAPI();
            api.RequestMatch(1, MatchType.Ranked, 1500f, (ip, port) => { });
            Assert.That(api.LastReqMatchType, Is.EqualTo(MatchType.Ranked));
        }

        [Test]
        public void RequestUnranked_SetsLastReqMatch_ToUnranked()
        {
            var api = new NoSkillBaseAPI();
            api.RequestMatch(1, MatchType.Unranked, 1500f, (ip, port) => { });
            Assert.That(api.LastReqMatchType, Is.EqualTo(MatchType.Unranked));
        }

        [Test]
        public void RequestNormal_SetsLastReqMatch_ToNormal()
        {
            var api = new NoSkillBaseAPI();
            api.RequestMatch(1, MatchType.Normal, 0f, (ip, port) => { });
            Assert.That(api.LastReqMatchType, Is.EqualTo(MatchType.Normal));
        }

        //! RequestMatch Callbacks

        [Test]
        public void RequestMatch_InvokesCallback_OnEachMatchType()
        {
            var api = new NoSkillBaseAPI();
            int callCount = 0;

            api.RequestMatch(1, MatchType.Ranked, 1500f, (ip, port) => callCount++);
            api.RequestMatch(1, MatchType.Unranked, 1500f, (ip, port) => callCount++);
            api.RequestMatch(1, MatchType.Normal, 0f, (ip, port) => callCount++);

            Assert.That(callCount, Is.EqualTo(3));
        }

        public void RequestMatch_NullCallback_ThrowsArgNullException()
        {
            var api = new NoSkillBaseAPI();

            Assert.Throws<ArgumentNullException>(
                () => api.RequestMatch(1, MatchType.Normal, 0f, null!)
            );
        }

        public void RequestMatch_SequentialCalls_MostRecentMatchTypeWins()
        {
            var api = new NoSkillBaseAPI();

            api.RequestMatch(1, MatchType.Ranked, 1500f, (ip, port) => { });
            Assert.That(api.LastReqMatchType, Is.EqualTo(MatchType.Ranked));

            api.RequestMatch(1, MatchType.Normal, 0f, (ip, port) => { });
            Assert.That(api.LastReqMatchType, Is.EqualTo(MatchType.Normal));
        }

        //! Submit Ranked Rating

        [Test]
        public void SubmitRating_NoThrows()
        {
            var api = new NoSkillBaseAPI();
            Assert.DoesNotThrow(() => api.SubmitRating(1, 500f));
        }

        [Test]
        public void SubmitRating_NegativeRating_NoThrows()
        {
            var api = new NoSkillBaseAPI();
            Assert.DoesNotThrow(() => api.SubmitRating(1, -500f));
        }

        //! Cancel Match Request

        [Test]
        public void CancelMatchRequest_NoThrows()
        {
            var api = new NoSkillBaseAPI();
            Assert.DoesNotThrow(() => api.CancelMatchRequest(1));
        }

        [Test]
        public void CancelMatchRequest_BeforeAnyRequestMade_NoThrows()
        {
            var api = new NoSkillBaseAPI();
            Assert.DoesNotThrow(() => api.CancelMatchRequest(999));
        }
    }
}