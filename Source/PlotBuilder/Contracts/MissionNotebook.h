#pragma once

#include "CoreMinimal.h"
#include "Contracts/ContractTypes.h"
#include "MissionNotebook.generated.h"

/** How a line of the notebook stands. It picks the icon and the colour: todo grey, carried orange, done and ok green, lost and warning red. */
UENUM(BlueprintType)
enum class ENotebookState : uint8
{
	/** Still to do (a piece of furniture still in the house). */
	Todo,
	/** In the player's hands. */
	Carried,
	/** Done (a piece delivered to the client zone). */
	Done,
	/** Gone for good (a piece of furniture destroyed). */
	Lost,
	/** Nothing wrong (a zone untouched, the forbidden tool not used, the room still standing). */
	Ok,
	/** Something went wrong (a zone damaged, the forbidden tool used). */
	Warning
};

/** The kinds of section of the notebook, in the order they are listed. */
UENUM(BlueprintType)
enum class ENotebookSection : uint8
{
	Furniture,
	Zones,
	KeepStanding,
	ForbiddenTool
};

/** One line of a section: a piece of furniture, a zone, the room to keep, the forbidden tool. */
USTRUCT(BlueprintType)
struct FNotebookLine
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString Name;

	/** What to say about it, as the player reads it ("Delivered, 90 %", "In your hands", "Damaged, 10 %"). */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString State;

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	ENotebookState Status = ENotebookState::Todo;

	/** Share of the thing that is damaged, 0 to 1, when a bar is worth drawing; -1 for no bar. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	float Damage = -1.f;

	/** A piece of furniture: the value the client counts for it over the value it asked for ("300 / 400"), after the health curve. Empty for the other lines. The debrief shows it. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString Figure;
};

/** A group of lines under one title. */
USTRUCT(BlueprintType)
struct FNotebookSection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	ENotebookSection Kind = ENotebookSection::Furniture;

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString Title;

	/** Short count next to the title ("1 / 3", "1 active"). Empty when there is nothing to count. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString Count;

	/** The section in one row, for the leave-the-site card: a name and what it comes to ("Furniture to recover", "2 of 3 delivered, the lamp still to deliver"). */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString SummaryName;

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString Summary;

	/** The furniture section only: what the client keeps over what it asked for ("550 kept of 730"). Empty for the other sections. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FString Figure;

	/** Ok when nothing is wrong or left to do, Warning otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	ENotebookState Status = ENotebookState::Ok;

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	TArray<FNotebookLine> Lines;
};

/**
 *  What the mission notebook and the leave-the-site card show: the contract, how much of the house is down, and one
 *  section per thing the contract asks for, each with its lines and their state right now. Plain data, built from the
 *  same snapshot as the report (MissionNotebook::Build), so it can be tested without a level and the two never disagree.
 */
USTRUCT(BlueprintType)
struct FNotebookData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	FContractDefinition Contract;

	/** Share of the target set that is down, 0 to 1: the "House demolished" ring. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	float Completion = 0.f;

	/** The pieces of furniture the client asked for that are neither delivered nor destroyed, so could still be carried to the client zone. */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	int32 PendingFurniture = 0;

	/**
	 *  Nothing is left to do: the whole target set is down and no requested piece could still be delivered. A destroyed piece does
	 *  not count (it cannot be recovered, asking about it would only be noise). Leaving a complete job needs no question.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	bool bJobComplete = true;

	UPROPERTY(BlueprintReadOnly, Category = "Notebook")
	TArray<FNotebookSection> Sections;

	/** The section of that kind, or nullptr. */
	const FNotebookSection* FindSection(ENotebookSection Kind) const
	{
		return Sections.FindByPredicate([Kind](const FNotebookSection& Section) { return Section.Kind == Kind; });
	}

	/** The notebook as text, for the log and the on-screen readout (pb.ShowNotebook). */
	FString ToString() const;
};

namespace MissionNotebook
{
	/**
	 *  The notebook for Contract against a mission as Snapshot says it stands. The furniture is listed in hands first, then still
	 *  to do, then delivered, then lost, so the first lines are the useful ones when the list scrolls. Zone damage is worked out
	 *  with the same function as the report, from Params.
	 */
	FNotebookData Build(const FContractDefinition& Contract, const FMissionSnapshot& Snapshot, const FScoringParams& Params);

	/** A room type as the player reads it: "LivingRoom" becomes "Living room". */
	FString RoomName(FName RoomType);
}
