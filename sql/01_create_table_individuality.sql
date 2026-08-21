-- ============================================================================
-- userIndividualityDB - Table
-- SQL Server 2022
-- ============================================================================

IF OBJECT_ID(N'dbo.userIndividualityDB', N'U') IS NOT NULL
    DROP TABLE dbo.userIndividualityDB;
GO

CREATE TABLE dbo.userIndividualityDB
(
    idx         INT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    accountIDX  INT             NOT NULL,
    ClassType   INT             NOT NULL,
    BasicTrait1 INT             NOT NULL DEFAULT 0,
    BasicTrait2 INT             NOT NULL DEFAULT 0,
    BasicTrait3 INT             NOT NULL DEFAULT 0,
    BasicTrait4 INT             NOT NULL DEFAULT 0,
    BasicTrait5 INT             NOT NULL DEFAULT 0,
    BasicTrait6 INT             NOT NULL DEFAULT 0,
    BasicTrait7 INT             NOT NULL DEFAULT 0,
    BasicTrait8 INT             NOT NULL DEFAULT 0,
    CoreTrait1  INT             NOT NULL DEFAULT 0,
    CoreTrait2  INT             NOT NULL DEFAULT 0,
    CoreTrait3  INT             NOT NULL DEFAULT 0,
    regDate     DATETIME        NOT NULL DEFAULT GETDATE()
);
GO

-- Index for user lookup
CREATE INDEX IX_userIndividualityDB_accountIDX
    ON dbo.userIndividualityDB (accountIDX);
GO
