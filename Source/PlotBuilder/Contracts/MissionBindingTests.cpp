#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Contracts/MissionBinding.h"

/**
 *  Which pieces make up a room, for the keep-standing condition: coordinates only, no actor. Run from the editor:
 *  Automation RunTests PlotBuilder.Contracts.Binding
 */
namespace PBBindingTests
{
	FBindingItem MakePiece(EPieceClass PieceClass, int32 X, int32 Y, int32 Z)
	{
		FBindingItem Item;
		Item.bIsProp = false;
		Item.bGridPiece = true;
		Item.PieceClass = PieceClass;
		Item.GridCoord = FIntVector(X, Y, Z);
		return Item;
	}
}

// A room of cells 1..2 by 1..2 on storey 0 has its perimeter at grid 1 and 5 on both axes (2*1-1 and 2*2+1). Its walls, its corner
// posts and its floor slab are the room's, and so is the slab right above it, strictly inside the perimeter: the ceiling.
// Not the wall of the next room, nor the walls of the storey above, nor the slab of the storey above that, nor a prop, nor a
// piece with no cell.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPBBindingRoomTest, "PlotBuilder.Contracts.Binding.RoomMembership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPBBindingRoomTest::RunTest(const FString& Parameters)
{
	FGridRoom Room;
	Room.Level = 0;
	Room.MinX = 2 * 1 - 1;
	Room.MaxX = 2 * 2 + 1;
	Room.MinY = 2 * 1 - 1;
	Room.MaxY = 2 * 2 + 1;

	TestTrue(TEXT("a wall on the west perimeter"), MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::InteriorWall, 1, 2, 0), Room));
	TestTrue(TEXT("a wall on the east perimeter"), MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::ExteriorWall, 5, 4, 0), Room));
	TestTrue(TEXT("a corner post"), MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::CornerPost, 1, 1, 0), Room));
	TestTrue(TEXT("the floor slab"), MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::FloorSlab, 2, 2, 0), Room));
	TestTrue(TEXT("the ceiling slab"), MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::FloorSlab, 4, 4, 1), Room));

	TestTrue(TEXT("not the wall of the next room"), !MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::InteriorWall, 7, 2, 0), Room));
	TestTrue(TEXT("not a wall of the storey above"), !MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::InteriorWall, 1, 2, 1), Room));
	TestTrue(TEXT("not a slab beside the room on the storey above"), !MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::FloorSlab, 6, 2, 1), Room));
	TestTrue(TEXT("not the slab two storeys up"), !MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::FloorSlab, 2, 2, 2), Room));
	TestTrue(TEXT("not a storey below"), !MissionBinding::IsPartOfRoom(PBBindingTests::MakePiece(EPieceClass::FloorSlab, 2, 2, -1), Room));

	FBindingItem Prop = PBBindingTests::MakePiece(EPieceClass::FloorSlab, 2, 2, 0);
	Prop.bIsProp = true;
	TestTrue(TEXT("not a prop"), !MissionBinding::IsPartOfRoom(Prop, Room));

	FBindingItem Legacy = PBBindingTests::MakePiece(EPieceClass::InteriorWall, 2, 2, 0);
	Legacy.bGridPiece = false;
	TestTrue(TEXT("not a piece with no cell"), !MissionBinding::IsPartOfRoom(Legacy, Room));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
