// ntrtl.h

#pragma once

//
// Errors
//

DECLSPEC_NORETURN
FORCEINLINE VOID NTAPI_INLINE RtlFailFast (
	_In_ ULONG Code
)
{
	__fastfail (Code);
}

DECLSPEC_NORETURN
FORCEINLINE VOID NTAPI_INLINE RtlFatalListEntryError (
	_In_ PVOID p1,
	_In_ PVOID p2,
	_In_ PVOID p3
)
{
	//++
	//    This routine reports a fatal list entry error.  It is implemented here as a
	//    wrapper around RtlFailFast so that alternative reporting mechanisms (such
	//    as simply logging and trying to continue) can be easily switched in.
	//--

	UNREFERENCED_PARAMETER (p1);
	UNREFERENCED_PARAMETER (p2);
	UNREFERENCED_PARAMETER (p3);

	RtlFailFast (FAST_FAIL_CORRUPT_LIST_ENTRY);
}

FORCEINLINE VOID NTAPI_INLINE RtlCheckListEntry (
	_In_ PLIST_ENTRY Entry
)
{
	if ((((Entry->Flink)->Blink) != Entry) || (((Entry->Blink)->Flink) != Entry))
		RtlFatalListEntryError ((PVOID)(Entry), (PVOID)((Entry->Flink)->Blink), (PVOID)((Entry->Blink)->Flink));
}

//
// Linked lists
//

FORCEINLINE VOID NTAPI_INLINE InitializeListHead (
	_Out_ PLIST_ENTRY ListHead
)
{
	ListHead->Flink = ListHead->Blink = ListHead;
}

_Must_inspect_result_
FORCEINLINE BOOLEAN NTAPI_INLINE
IsListEmpty (
	_In_ PLIST_ENTRY ListHead
)
{
	return (ListHead->Flink == ListHead);
}

FORCEINLINE BOOLEAN NTAPI_INLINE RemoveEntryList (
	_In_ PLIST_ENTRY Entry
)
{
	PLIST_ENTRY PrevEntry;
	PLIST_ENTRY NextEntry;

	NextEntry = Entry->Flink;
	PrevEntry = Entry->Blink;

	if ((NextEntry->Blink != Entry) || (PrevEntry->Flink != Entry))
		RtlFatalListEntryError ((PVOID)PrevEntry, (PVOID)Entry, (PVOID)NextEntry);

	PrevEntry->Flink = NextEntry;
	NextEntry->Blink = PrevEntry;

	return (NextEntry == PrevEntry);
}

FORCEINLINE PLIST_ENTRY NTAPI_INLINE RemoveHeadList (
	_Inout_ PLIST_ENTRY ListHead
)
{
	PLIST_ENTRY NextEntry;
	PLIST_ENTRY Entry;

	Entry = ListHead->Flink;
	NextEntry = Entry->Flink;

	if ((Entry->Blink != ListHead) || (NextEntry->Blink != Entry))
		RtlFatalListEntryError ((PVOID)ListHead, (PVOID)Entry, (PVOID)NextEntry);

	ListHead->Flink = NextEntry;
	NextEntry->Blink = ListHead;

	return Entry;
}

FORCEINLINE PLIST_ENTRY NTAPI_INLINE RemoveTailList (
	_Inout_ PLIST_ENTRY ListHead
)
{
	PLIST_ENTRY PrevEntry;
	PLIST_ENTRY Entry;

	Entry = ListHead->Blink;
	PrevEntry = Entry->Blink;

	if ((Entry->Flink != ListHead) || (PrevEntry->Flink != Entry))
		RtlFatalListEntryError ((PVOID)PrevEntry, (PVOID)Entry, (PVOID)ListHead);

	ListHead->Blink = PrevEntry;
	PrevEntry->Flink = ListHead;

	return Entry;
}

FORCEINLINE VOID NTAPI_INLINE InsertTailList (
	_Inout_ PLIST_ENTRY ListHead,
	_Inout_ __drv_aliasesMem PLIST_ENTRY Entry
)
{
	PLIST_ENTRY PrevEntry;

	PrevEntry = ListHead->Blink;

	if (PrevEntry->Flink != ListHead)
		RtlFatalListEntryError ((PVOID)PrevEntry, (PVOID)ListHead, (PVOID)PrevEntry->Flink);

	Entry->Flink = ListHead;
	Entry->Blink = PrevEntry;
	PrevEntry->Flink = Entry;
	ListHead->Blink = Entry;
}

FORCEINLINE VOID NTAPI_INLINE InsertHeadList (
	_Inout_ PLIST_ENTRY ListHead,
	_Inout_ __drv_aliasesMem PLIST_ENTRY Entry
)
{
	PLIST_ENTRY NextEntry;

	NextEntry = ListHead->Flink;

	RtlCheckListEntry (ListHead);

	if (NextEntry->Blink != ListHead)
	{
		RtlFatalListEntryError ((PVOID)ListHead, (PVOID)NextEntry, (PVOID)NextEntry->Blink);
	}

	Entry->Flink = NextEntry;
	Entry->Blink = ListHead;
	NextEntry->Blink = Entry;
	ListHead->Flink = Entry;
}

FORCEINLINE VOID NTAPI_INLINE AppendTailList (
	_Inout_ PLIST_ENTRY ListHead,
	_Inout_ PLIST_ENTRY ListToAppend
)
{
	PLIST_ENTRY ListEnd = ListHead->Blink;

	RtlCheckListEntry (ListHead);
	RtlCheckListEntry (ListToAppend);

	ListHead->Blink->Flink = ListToAppend;
	ListHead->Blink = ListToAppend->Blink;
	ListToAppend->Blink->Flink = ListHead;
	ListToAppend->Blink = ListEnd;
}

FORCEINLINE PSINGLE_LIST_ENTRY NTAPI_INLINE PopEntryList (
	_Inout_ PSINGLE_LIST_ENTRY ListHead
)
{
	PSINGLE_LIST_ENTRY FirstEntry;

	FirstEntry = ListHead->Next;

	if (FirstEntry)
		ListHead->Next = FirstEntry->Next;

	return FirstEntry;
}

FORCEINLINE VOID NTAPI_INLINE PushEntryList (
	_Inout_ PSINGLE_LIST_ENTRY ListHead,
	_Inout_ __drv_aliasesMem PSINGLE_LIST_ENTRY Entry
)
{
	Entry->Next = ListHead->Next;
	ListHead->Next = Entry;
}
