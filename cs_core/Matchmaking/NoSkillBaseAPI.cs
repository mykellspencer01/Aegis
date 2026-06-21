/*
    * Handles genre selection for a session and provides stubs for match requests

    ! Responsibilities
        * Genre Activation
        * Match Type Selection
            > Normal
            > Unranked
            > Ranked
        * Rank and Match 
*/

using System;
using Aegis.Managed.Interop;

namespace Aegis.Managed.Matchmaking
{
    public enum GenreID : byte
    {
        None = 0,
        FPS = 1,
        RTS = 2,
        MMO = 3,
        Custom = 4
    }

    public enum MatchType
    {
        Normal = 0,
        Unranked = 1,
        Ranked = 2,
    }

    public class NoSkillBaseAPI
    {
        // * State
        public GenreID ActiveGenre { get; private set;} = GenreID.None;
        public MatchType LastReqMatchType { get; private set; } = MatchType.Normal;

        // * Genre Activation
        public NetResult SetGenre(GenreID genre)
        {
            NetResult result = (NetResult)NativeBindings.Aegis_SetGenre((byte)genre);
            if(result == NetResult.Ok) ActiveGenre = genre;
            return result;
        }

        // * Rank/Rating
        public void SubmitRating(ulong playerID, float rating)
        {
            Console.WriteLine(
                $"[Matchmaking] SubmitRating: player = {playerID} " +
                $"Rating = {rating:F1}, Genre = {ActiveGenre}, Match Type = {LastReqMatchType}"
            );
        }

        // * Match Requests
        public void RequestMatch(ulong playerID, MatchType match, float rating, Action<string, ushort> onMatchFound)
        {
            if(onMatchFound == null) throw new ArgumentNullException(nameof(onMatchFound));
            LastReqMatchType = match;

            string ratingInfo = match == MatchType.Normal ? "ignored" : rating.ToString("F1");
            Console.WriteLine(
                $"[Matchmaking] Request Match: Player = {playerID}, Genre = {ActiveGenre}, " +
                $"Match Type = {match}, Rating = {ratingInfo}."
            );

            onMatchFound("127.0.0.1", 7777);
        }

        public void RequestNormalMatch(ulong playerID, Action<string, ushort> onMatchFound)
        {
            RequestMatch(playerID, MatchType.Normal, rating: 0f, onMatchFound);
        }

        public void CancelMatchRequest(ulong playerID)
        {
            Console.WriteLine(
                $"[Matchmaking] Cancel Match Request: Player = {playerID} " +
                $"Match Type = {LastReqMatchType}."
            );
        }
    }
}