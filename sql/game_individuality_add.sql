-- ============================================================================
-- game_individuality_add (queryId 2207)
-- INSERT new individuality data
-- Called by: DBClient::OnInsertIndividuality
-- Params: accountIDX, ClassType, BasicTrait1-8, CoreTrait1-3
-- regDate is set automatically by DEFAULT GETDATE()
-- ============================================================================

IF OBJECT_ID(N'dbo.game_individuality_add', N'P') IS NOT NULL
    DROP PROCEDURE dbo.game_individuality_add;
GO

CREATE PROCEDURE dbo.game_individuality_add
    @accountIDX  INT,
    @ClassType   INT,
    @BasicTrait1 INT = 0,
    @BasicTrait2 INT = 0,
    @BasicTrait3 INT = 0,
    @BasicTrait4 INT = 0,
    @BasicTrait5 INT = 0,
    @BasicTrait6 INT = 0,
    @BasicTrait7 INT = 0,
    @BasicTrait8 INT = 0,
    @CoreTrait1  INT = 0,
    @CoreTrait2  INT = 0,
    @CoreTrait3  INT = 0
AS
BEGIN
    SET NOCOUNT ON;

    INSERT INTO dbo.userIndividualityDB
    (
        accountIDX,
        ClassType,
        BasicTrait1, BasicTrait2, BasicTrait3, BasicTrait4,
        BasicTrait5, BasicTrait6, BasicTrait7, BasicTrait8,
        CoreTrait1, CoreTrait2, CoreTrait3
    )
    VALUES
    (
        @accountIDX,
        @ClassType,
        @BasicTrait1, @BasicTrait2, @BasicTrait3, @BasicTrait4,
        @BasicTrait5, @BasicTrait6, @BasicTrait7, @BasicTrait8,
        @CoreTrait1, @CoreTrait2, @CoreTrait3
    );
END;
GO
