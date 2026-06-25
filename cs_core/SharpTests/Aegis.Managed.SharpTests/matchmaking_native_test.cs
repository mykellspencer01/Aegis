using NUnit.Framework;
using Aegis.Managed.Session;
using Aegis.Managed.Matchmaking;
using Aegis.Managed.Interop;

namespace Aegis.Managed.SharpTests
{
    [TestFixture]
    public class MatchmakingNativeTests
    {
        //! SetGenre

        [Test]
        public void SetGenre_AfterHost_FPSReturnsOk()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var api = new NoSkillBaseAPI();
                NetResult result = api.SetGenre(GenreID.FPS);

                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void SetGenre_AfterHost_FPS_UpdatesActiveGenre()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var api = new NoSkillBaseAPI();
                api.SetGenre(GenreID.FPS);

                Assert.That(api.ActiveGenre, Is.EqualTo(GenreID.FPS));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void SetGenre_AfterHost_RTS_ReturnsOk()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var api = new NoSkillBaseAPI();
                NetResult result = api.SetGenre(GenreID.RTS);

                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void SetGenre_AfterHost_MMO_ReturnsOk()
        {
            var session = new SessionManager();

            try
            {
                session.Host(0);
                var api = new NoSkillBaseAPI();
                NetResult result = api.SetGenre(GenreID.MMO);

                Assert.That(result, Is.EqualTo(NetResult.Ok));
            }
            finally { session.Shutdown(); }
        }

        [Test]
        public void SetGenre_BeforeHost_ReturnsNotInitialized()
        {
            var api = new NoSkillBaseAPI();
            NetResult result = api.SetGenre(GenreID.FPS);

            Assert.That(result, Is.EqualTo(NetResult.NotInitialized));
        }

        [Test]
        public void SetGenre_BeforeHost_DoesNotUpdate_ActiveGenre()
        {
            var api = new NoSkillBaseAPI();
            NetResult result = api.SetGenre(GenreID.FPS);

            Assert.That(api.ActiveGenre, Is.EqualTo(GenreID.None));
        }

        [Test]
        public void SetGenre_CalledTwiceDiffGenres_SecondCallSucceeds()
        {
            var session = new SessionManager();
            try
            {
                session.Host(0);
                var api = new NoSkillBaseAPI();
                api.SetGenre(GenreID.FPS);
                Assert.That(api.ActiveGenre, Is.EqualTo(GenreID.FPS));

                api.SetGenre(GenreID.MMO);
                Assert.That(api.ActiveGenre, Is.EqualTo(GenreID.MMO));
            }
            finally { session.Shutdown(); }
        }
    }
}