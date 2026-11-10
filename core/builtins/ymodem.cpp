// ymodem.cpp
// 
// Copyright (c) 2008 - 2010 Accenture. All rights reserved.
// This component and the accompanying materials are made available
// under the terms of the "Eclipse Public License v2.0"
// which accompanies this distribution, and is available
// at the URL "https://www.eclipse.org/legal/epl-2.0/".
// SPDX-License-Identifier: EPL-2.0
// Initial Contributors:
// Accenture - Initial contribution
// The NeoSymbian Authors (2026): verified UTF-8 file transfer.

#include <fshell/ioutils.h>
#include "ymodem.h"
#include "ymodem_meta.h"
#include <fshell/ltkutils.h>
#include <fshell/common.mmh>
#if defined(FSHELL_CRYPTO_SUPPORT) && defined(FSHELL_SHA2_SUPPORT)
#include <hash.h>
#endif

using namespace IoUtils;

CCommandBase* CCmdYmodem::NewLC()
	{
	CCmdYmodem* self = new(ELeave) CCmdYmodem();
	CleanupStack::PushL(self);
	self->BaseConstructL();
	self->ConstructL();
	return self;
	}

CCmdYmodem::~CCmdYmodem()
	{
	iFileNames.Close();
	delete iExpectedSha256;
	}

CCmdYmodem::CCmdYmodem()
	{
	iBlockSize = 1024;
	}

const TDesC& CCmdYmodem::Name() const
	{
	_LIT(KName, "ymodem");	
	return KName;
	}

namespace
	{
	struct TReceiveCleanup
		{
		RFs& iFs;
		RFile iFile;
		TFileName iTemporary;
		TReceiveCleanup(RFs& aFs) : iFs(aFs) {}
		};
	void CleanupReceive(TAny* aPtr)
		{
		TReceiveCleanup& c = *static_cast<TReceiveCleanup*>(aPtr);
		c.iFile.Close();
		if (c.iTemporary.Length()) c.iFs.Delete(c.iTemporary);
		}
	TInt HexDigit(TUint aChar)
		{
		if (aChar >= '0' && aChar <= '9') return aChar - '0';
		if (aChar >= 'a' && aChar <= 'f') return aChar - 'a' + 10;
		if (aChar >= 'A' && aChar <= 'F') return aChar - 'A' + 10;
		return -1;
		}
	}

void CCmdYmodem::ReceiveFileL(const TFileName2& aName, TInt aSize)
	{
	// Never receive directly onto an existing destination. RFs::Replace is
	// requested only after complete, flushed, optionally hash-verified input.
	if (!iOverwrite)
		{
		TEntry entry;
		TInt err = FsL().Entry(aName, entry);
		if (err == KErrNone) User::Leave(KErrAlreadyExists);
		if (err != KErrNotFound && err != KErrPathNotFound) User::Leave(err);
		}
	TReceiveCleanup cleanup(FsL());
	CleanupStack::PushL(TCleanupItem(&CleanupReceive, &cleanup));
	User::LeaveIfError(cleanup.iFile.Temp(FsL(), aName.DriveAndPath(), cleanup.iTemporary, EFileWrite));
#if defined(FSHELL_CRYPTO_SUPPORT) && defined(FSHELL_SHA2_SUPPORT)
	CMessageDigest* digest = NULL;
	if (iExpectedSha256)
		{
		digest = CMessageDigestFactory::NewDigestL(CMessageDigest::ESHA256);
		CleanupStack::PushL(digest);
		}
#endif
	TInt remaining = aSize;
	TBool firstBlock = ETrue, finalBlock = EFalse;
	if (SendSyncL() == EEot)
		{
		if (remaining) User::Leave(KErrCorrupt);
		finalBlock = ETrue;
		}
	while (!finalBlock)
		{
		TPtrC block(ReceiveBlockL(firstBlock, finalBlock));
		firstBlock = EFalse;
		if (finalBlock)
			{
			if (remaining) User::Leave(KErrCorrupt);
			break;
			}
		if (!block.Length()) continue; // acknowledged duplicate, not new bytes
		if (!remaining) User::Leave(KErrCorrupt); // an extra data block
		const TInt count = Min(remaining, block.Length());
		TBuf8<1024> bytes;
		bytes.Copy(block.Left(count));
		User::LeaveIfError(cleanup.iFile.Write(bytes));
		remaining -= count;
		}
	User::LeaveIfError(cleanup.iFile.SetSize(aSize));
	User::LeaveIfError(cleanup.iFile.Flush());
	cleanup.iFile.Close();
#if defined(FSHELL_CRYPTO_SUPPORT) && defined(FSHELL_SHA2_SUPPORT)
	if (digest)
		{
		// Verify the flushed file through F32, not just the incoming wire bytes.
		RFile verify;
		User::LeaveIfError(verify.Open(FsL(), cleanup.iTemporary, EFileRead | EFileShareReadersOnly));
		CleanupClosePushL(verify);
		TBuf8<1024> bytes;
		TInt checked = 0;
		for (;;)
			{
			User::LeaveIfError(verify.Read(bytes));
			if (!bytes.Length()) break;
			if (bytes.Length() > aSize - checked) User::Leave(KErrCorrupt);
			digest->Update(bytes);
			checked += bytes.Length();
			}
		if (checked != aSize || digest->Final().Compare(iExpectedDigest) != 0) User::Leave(KErrCorrupt);
		CleanupStack::PopAndDestroy(&verify);
		CleanupStack::PopAndDestroy(digest);
		}
#endif
	User::LeaveIfError(iOverwrite ? FsL().Replace(cleanup.iTemporary, aName) : FsL().Rename(cleanup.iTemporary, aName));
	cleanup.iTemporary.Zero();
	CleanupStack::PopAndDestroy(&cleanup);
	}

void CCmdYmodem::DoRunL()
	{
	if (iExpectedSha256)
		{
#if !defined(FSHELL_CRYPTO_SUPPORT) || !defined(FSHELL_SHA2_SUPPORT)
		User::Leave(KErrNotSupported);
#else
		if (iMode != EReceive || iExpectedSha256->Length() != 64) User::Leave(KErrArgument);
		for (TInt i = 0; i < 64; i += 2)
			{
			TInt hi = HexDigit((*iExpectedSha256)[i]), lo = HexDigit((*iExpectedSha256)[i + 1]);
			if (hi < 0 || lo < 0) User::Leave(KErrArgument);
			iExpectedDigest.Append((hi << 4) | lo);
			}
#endif
		}
	if (iMode == ESend && iOutputFileName.Length()) User::Leave(KErrArgument);
	if (iMode == EReceive && iFileNames.Count() > 1) User::Leave(KErrArgument);
	PrepareConsoleToTransferL();
	if (iMode == EReceive)
		{
		TFileName2 receiveDir(iFileNames.Count() ? iFileNames[0] : Env().Pwd());
		if (!receiveDir.IsDirL(FsL())) User::Leave(KErrPathNotFound);
		if (iOutputFileName.Length()) iOutputFileName.MakeAbsoluteL(Env().Pwd());
		TInt received = 0;
		for (;;)
			{
			if (SendSyncL() == EEot) User::Leave(KErrCorrupt);
			iPacketNumber = 0;
			TBool finalBlock = EFalse;
			TPtrC block(ReceiveBlockL(ETrue, finalBlock));
			if (finalBlock) User::Leave(KErrCorrupt);
			NeoYmodem::THeader header;
			const TInt parsed = NeoYmodem::ParseHeader(block.Ptr(), block.Length(), header);
			if (parsed < 0) User::Leave(KErrCorrupt);
			if (parsed == 0) break;
			if (received && (iExpectedSha256 || iOutputFileName.Length())) User::Leave(KErrArgument);
			TBuf8<1024> encodedName;
			encodedName.Copy(block.Left(header.iNameBytes));
			HBufC* decodedName = LtkUtils::DecodeUtf8L(encodedName);
			CleanupStack::PushL(decodedName);
			TFileName2 destination;
			if (iOutputFileName.Length()) destination = iOutputFileName;
			else
				{
				if (receiveDir.Length() + decodedName->Length() + 1 > destination.MaxLength()) User::Leave(KErrBadName);
				destination = *decodedName;
				destination.MakeAbsoluteL(receiveDir);
				}
			CleanupStack::PopAndDestroy(decodedName);
			ReceiveFileL(destination, header.iSize);
			++received;
			}
		if (!received && (iExpectedSha256 || iOutputFileName.Length())) User::Leave(KErrCorrupt);
		CleanupClonsoleAfterTransferL();
		Printf(_L("Successfully received %d file(s).\r\n"), received);
		}
	else if (iMode == ESend)
		{
		const TInt numFiles = iFileNames.Count();
		if (!numFiles) User::Leave(KErrArgument);
		WaitForSyncL();
		HBufC* buf = HBufC::NewLC(1024);
		TPtr bufPtr(buf->Des());
		for (TInt i = 0; i < numFiles; ++i)
			{
			const TFileName2& fileName = iFileNames[i];
			TEntry entry;
			User::LeaveIfError(FsL().Entry(fileName, entry));
			if (entry.IsDir() || entry.iSize < 0) User::Leave(KErrArgument);
			HBufC8* encodedName = LtkUtils::Utf8L(fileName.NameAndExt());
			CleanupStack::PushL(encodedName);
			if (encodedName->Length() + 13 > 1024) User::Leave(KErrBadName);
			iBlockSize = 1024;
			bufPtr.Copy(*encodedName);
			bufPtr.Append(0);
			bufPtr.AppendNum(entry.iSize);
			bufPtr.AppendFill(0, 1024 - bufPtr.Length());
			NeoYmodem::THeader header;
			if (NeoYmodem::ParseHeader(bufPtr.Ptr(), bufPtr.Length(), header) != 1) User::Leave(KErrBadName);
			CleanupStack::PopAndDestroy(encodedName);
			iPacketNumber = 0;
			SendBlockL(bufPtr);
			WaitForSyncL();
			SendFileL(fileName);
			WaitForSyncL();
			}
		iPacketNumber = 0;
		iBlockSize = 128;
		bufPtr.SetLength(128);
		bufPtr.FillZ();
		SendBlockL(bufPtr);
		CleanupStack::PopAndDestroy(buf);
		CleanupClonsoleAfterTransferL();
		Printf(_L("Successfully sent %d file(s).\r\n"), numFiles);
		}
	User::LeaveIfError(Stdin().CancelCaptureAllKeys());
	}

void CCmdYmodem::ArgumentsL(RCommandArgumentList& aArguments)
	{
	_LIT(KArg1, "mode");
	aArguments.AppendEnumL((TInt&)iMode, KArg1);
	_LIT(KArg2, "file_name");
	aArguments.AppendFileNameL(iFileNames, KArg2);
	}

void CCmdYmodem::OptionsL(RCommandOptionList& aOptions)
	{
	CCmdXmodem::OptionsL(aOptions);
	aOptions.AppendFileNameL(iOutputFileName, _L("output"));
	aOptions.AppendStringL(iExpectedSha256, _L("sha256"));
	}

