using EppoScriptCore.Core;
using EppoScriptCore.Scene;

namespace Chess
{
    // Drives the whole game: board view, player input and the bot.
    public class ChessGame : Entity
    {
        public override void OnCreate()
        {
            Log.Info("Chess: game started");
        }

        public override void OnUpdate(float timestep)
        {
        }
    }
}
